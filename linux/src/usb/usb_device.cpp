#include "phonebridge/usb_device.h"
#include <libusb-1.0/libusb.h>
#include <cstring>
#include <cstdio>

namespace phonebridge {

struct UsbDevice::Impl {
    libusb_context* context = nullptr;
    libusb_device_handle* handle = nullptr;
    libusb_device* device = nullptr;
    int outEndpoint = -1;
    int inEndpoint = -1;
    int interfaceNum = -1;
    uint32_t sequence = 0;
    UsbDeviceInfo deviceInfo;
    std::thread receiveThread;
    std::atomic<bool> receiving{false};
};

UsbDevice::UsbDevice() : impl_(new Impl()) {}

UsbDevice::~UsbDevice() {
    shutdown();
    delete impl_;
}

bool UsbDevice::initialize() {
    int result = libusb_init(&impl_->context);
    if (result != 0) {
        if (onError) onError("Failed to initialize libusb: " + std::string(libusb_error_name(result)));
        return false;
    }
    return true;
}

void UsbDevice::shutdown() {
    disconnect();
    if (impl_->context) {
        libusb_exit(impl_->context);
        impl_->context = nullptr;
    }
}

bool UsbDevice::scanForDevice() {
    if (!impl_->context) return false;

    libusb_device** devices;
    ssize_t count = libusb_get_device_list(impl_->context, &devices);
    if (count < 0) {
        if (onError) onError("Failed to get USB device list");
        return false;
    }

    for (ssize_t i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devices[i], &desc) != 0) continue;

        if (desc.idVendor == AOA_VID) {
            bool isAoa = (desc.idProduct == AOA_PID ||
                          desc.idProduct == AOA_ACCESSORY_PID ||
                          desc.idProduct == AOA_ADB_PID ||
                          desc.idProduct == AOA_AUDIO_PID ||
                          desc.idProduct == AOA_AUDIO_ADB_PID ||
                          desc.idProduct == AOA_AUDIO_ACCESSORY_PID);
            if (isAoa) {
                impl_->deviceInfo.vendorId = desc.idVendor;
                impl_->deviceInfo.productId = desc.idProduct;
                impl_->deviceInfo.isAoaDevice = true;
                impl_->device = devices[i];
                libusb_ref_device(impl_->device);
                libusb_free_device_list(devices, 1);
                return true;
            }
        }
    }

    libusb_free_device_list(devices, 1);
    return false;
}

bool UsbDevice::openDevice() {
    if (!impl_->device) return false;

    int result = libusb_open(impl_->device, &impl_->handle);
    if (result != 0) {
        if (onError) onError("Failed to open USB device: " + std::string(libusb_error_name(result)));
        return false;
    }

    return true;
}

bool UsbDevice::findEndpoints() {
    if (!impl_->handle || !impl_->device) return false;

    struct libusb_config_descriptor* config;
    if (libusb_get_active_config_descriptor(impl_->device, &config) != 0) {
        if (onError) onError("Failed to get config descriptor");
        return false;
    }

    for (int i = 0; i < config->bNumInterfaces; i++) {
        const struct libusb_interface* iface = &config->interface[i];
        for (int j = 0; j < iface->num_altsetting; j++) {
            const struct libusb_interface_descriptor* alt = &iface->altsetting[j];
            impl_->inEndpoint = -1;
            impl_->outEndpoint = -1;

            for (int k = 0; k < alt->bNumEndpoints; k++) {
                const struct libusb_endpoint_descriptor* ep = &alt->endpoint[k];
                if ((ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
                    if (ep->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                        impl_->inEndpoint = ep->bEndpointAddress;
                    } else {
                        impl_->outEndpoint = ep->bEndpointAddress;
                    }
                }
            }

            if (impl_->inEndpoint >= 0 && impl_->outEndpoint >= 0) {
                impl_->interfaceNum = alt->bInterfaceNumber;
                libusb_free_config_descriptor(config);
                return true;
            }
        }
    }

    libusb_free_config_descriptor(config);
    if (onError) onError("No bulk endpoints found");
    return false;
}

bool UsbDevice::claimInterface() {
    if (!impl_->handle || impl_->interfaceNum < 0) return false;

    if (libusb_claim_interface(impl_->handle, impl_->interfaceNum) != 0) {
        if (onError) onError("Failed to claim USB interface");
        return false;
    }

    return true;
}

bool UsbDevice::connect() {
    if (impl_->handle) return true;

    if (!openDevice()) return false;
    if (!findEndpoints()) { disconnect(); return false; }
    if (!claimInterface()) { disconnect(); return false; }

    if (onConnected) onConnected();
    return true;
}

void UsbDevice::disconnect() {
    stopReceiveLoop();

    if (impl_->handle) {
        if (impl_->interfaceNum >= 0) {
            libusb_release_interface(impl_->handle, impl_->interfaceNum);
        }
        libusb_close(impl_->handle);
        impl_->handle = nullptr;
    }

    if (impl_->device) {
        libusb_unref_device(impl_->device);
        impl_->device = nullptr;
    }

    impl_->outEndpoint = -1;
    impl_->inEndpoint = -1;
    impl_->interfaceNum = -1;

    if (onDisconnected) onDisconnected();
}

bool UsbDevice::isConnected() const {
    return impl_->handle != nullptr;
}

const UsbDeviceInfo& UsbDevice::getDeviceInfo() const {
    return impl_->deviceInfo;
}

uint32_t UsbDevice::nextSequence() {
    return impl_->sequence++;
}

bool UsbDevice::sendRaw(const uint8_t* data, size_t length) {
    if (!impl_->handle || impl_->outEndpoint < 0) return false;

    int transferred = 0;
    int result = libusb_bulk_transfer(
        impl_->handle, impl_->outEndpoint,
        const_cast<uint8_t*>(data),
        static_cast<int>(length),
        &transferred, USB_TIMEOUT_MS
    );

    if (result != 0 && result != LIBUSB_ERROR_TIMEOUT) {
        if (onError) onError("USB send failed: " + std::string(libusb_error_name(result)));
        return false;
    }

    return static_cast<size_t>(transferred) == length;
}

bool UsbDevice::receiveRaw(uint8_t* data, size_t maxLength, size_t& actualLength, int timeoutMs) {
    if (!impl_->handle || impl_->inEndpoint < 0) return false;

    int transferred = 0;
    int result = libusb_bulk_transfer(
        impl_->handle, impl_->inEndpoint,
        data, static_cast<int>(maxLength),
        &transferred, timeoutMs
    );

    if (result == LIBUSB_ERROR_TIMEOUT) {
        actualLength = 0;
        return true;
    }

    if (result != 0) {
        if (result != LIBUSB_ERROR_NO_DEVICE) {
            if (onError) onError("USB receive failed: " + std::string(libusb_error_name(result)));
        }
        return false;
    }

    actualLength = static_cast<size_t>(transferred);
    return true;
}

bool UsbDevice::sendPacket(MessageType type, uint16_t flags, uint32_t sequence,
                           const uint8_t* payload, uint32_t payloadLength) {
    auto packet = Protocol::buildPacket(type, flags, sequence, payload, payloadLength);
    return sendRaw(packet.data(), packet.size());
}

void UsbDevice::receiveLoop() {
    uint8_t buffer[USB_BUFFER_SIZE];

    while (impl_->receiving.load()) {
        size_t actualLength = 0;
        if (!receiveRaw(buffer, sizeof(buffer), actualLength, 100)) {
            break;
        }

        if (actualLength == 0) continue;

        if (actualLength < sizeof(PacketHeader)) {
            continue;
        }

        if (!Protocol::validatePacket(buffer, actualLength)) {
            if (onError) onError("Received invalid packet");
            continue;
        }

        PacketHeader header = Protocol::parseHeader(buffer);
        const uint8_t* payload = buffer + sizeof(PacketHeader);

        if (onPacketReceived) {
            onPacketReceived(header, payload, header.payloadLength);
        }
    }
}

bool UsbDevice::startReceiveLoop() {
    if (!impl_->handle) return false;
    if (impl_->receiving.load()) return true;

    impl_->receiving.store(true);
    impl_->receiveThread = std::thread(&UsbDevice::receiveLoop, this);
    return true;
}

void UsbDevice::stopReceiveLoop() {
    impl_->receiving.store(false);
    if (impl_->receiveThread.joinable()) {
        impl_->receiveThread.join();
    }
}

} // namespace phonebridge

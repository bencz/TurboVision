#include "turbovision/core/frame_data.hpp"
#include <cstring>

namespace turbovision {
    FrameData::FrameData(int width, int height, AVPixelFormat format)
        : width_(width)
          , height_(height)
          , format_(format)
          , timestamp_(0) {
        // Calcular o tamanho necessário do buffer baseado no formato
        dataSize_ = av_image_get_buffer_size(format, width, height, 1);
        data_ = new uint8_t[dataSize_];
        calculatePlaneOffsets();
    }

    FrameData::~FrameData() {
        delete[] data_;
    }

    FrameData::FrameData(FrameData &&other) noexcept
        : data_(other.data_)
          , width_(other.width_)
          , height_(other.height_)
          , format_(other.format_)
          , timestamp_(other.timestamp_)
          , dataSize_(other.dataSize_) {
        other.data_ = nullptr;
        other.dataSize_ = 0;
    }

    FrameData &FrameData::operator=(FrameData &&other) noexcept {
        if (this != &other) {
            delete[] data_;

            data_ = other.data_;
            width_ = other.width_;
            height_ = other.height_;
            format_ = other.format_;
            timestamp_ = other.timestamp_;
            dataSize_ = other.dataSize_;

            other.data_ = nullptr;
            other.dataSize_ = 0;
        }
        return *this;
    }

    bool FrameData::copyFrom(const uint8_t *src, int size) {
        if (!src || size != dataSize_) {
            return false;
        }
        std::memcpy(data_, src, size);
        return true;
    }

    bool FrameData::copyTo(uint8_t *dst, int size) const {
        if (!dst || size != dataSize_) {
            return false;
        }
        std::memcpy(dst, data_, size);
        return true;
    }

    void FrameData::calculatePlaneOffsets() {
        planeOffsets_[0] = 0; // Y sempre começa em 0

        switch (format_) {
            case AV_PIX_FMT_NV12:
                planeOffsets_[1] = width_ * height_; // UV começa após Y
                break;
            case AV_PIX_FMT_YUV420P:
                planeOffsets_[1] = width_ * height_; // U
                planeOffsets_[2] = planeOffsets_[1] + (width_ * height_ / 4); // V
                break;
            // Adicione outros formatos conforme necessário
        }
    }

    int FrameData::calculateTotalSize() const {
        switch (format_) {
            case AV_PIX_FMT_NV12:
            case AV_PIX_FMT_YUV420P:
                return width_ * height_ * 3 / 2; // Y + UV
            case AV_PIX_FMT_YUV444P:
                return width_ * height_ * 3; // Y + U + V
            default:
                return width_ * height_ * 4; // Assume RGBA por segurança
        }
    }
} // namespace turbovision

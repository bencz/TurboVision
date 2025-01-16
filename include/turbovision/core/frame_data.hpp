#pragma once

#include "common.hpp"
#include <memory>

namespace turbovision {

    class TURBOVISION_API FrameData {
    public:
        // Construtor e Destrutor
        FrameData(int width, int height, AVPixelFormat format);
        ~FrameData();

        // Previne cópia
        FrameData(const FrameData&) = delete;
        FrameData& operator=(const FrameData&) = delete;

        // Permite movimento
        FrameData(FrameData&& other) noexcept;
        FrameData& operator=(FrameData&& other) noexcept;

        // Getters
        uint8_t* data() const { return data_; }
        int width() const { return width_; }
        int height() const { return height_; }
        AVPixelFormat format() const { return format_; }
        int64_t timestamp() const { return timestamp_; }
        int dataSize() const { return dataSize_; }

        // Setters
        void setTimestamp(int64_t ts) { timestamp_ = ts; }

        // Métodos de utilidade
        bool copyFrom(const uint8_t* src, int size);
        bool copyTo(uint8_t* dst, int size) const;

        void calculatePlaneOffsets();

        int calculateTotalSize() const;

        int getPlanOffset(int plane) const {
            if (plane >= 0 && plane < MAX_PLANES && planeOffsets_[plane] >= 0) {
                return planeOffsets_[plane];
            }
            return 0;
        }

    private:
        uint8_t* data_;
        int width_;
        int height_;
        AVPixelFormat format_;
        int64_t timestamp_;
        int dataSize_;

        static const int MAX_PLANES = 4;
        int planeOffsets_[MAX_PLANES] = {-1, -1, -1, -1};
    };

    using FramePtr = std::shared_ptr<FrameData>;

} // namespace turbovision
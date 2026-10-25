#include "AC/Core/Model.hpp"

#include "AC/Core/Internal/Model/Param.hpp"

template <typename Model>
ac::core::model::BaseModel<Model>::BaseModel() noexcept : blockNum(0), kptr(nullptr), bptr(nullptr), aptr(nullptr) {}

ac::core::model::ACNetLegacy::ACNetLegacy(const Variant v) noexcept
{
    this->blockNum = 8;
    switch (v)
    {
    case Variant::GAN:
        this->kptr = param::ACNetLegacy_GAN_NHWC_kernels;
        this->bptr = param::ACNetLegacy_GAN_NHWC_biases;
        break;
    case Variant::HDN0:
        this->kptr = param::ACNetLegacy_HDN0_NHWC_kernels;
        this->bptr = param::ACNetLegacy_HDN0_NHWC_biases;
        break;
    case Variant::HDN1:
        this->kptr = param::ACNetLegacy_HDN1_NHWC_kernels;
        this->bptr = param::ACNetLegacy_HDN1_NHWC_biases;
        break;
    case Variant::HDN2:
        this->kptr = param::ACNetLegacy_HDN2_NHWC_kernels;
        this->bptr = param::ACNetLegacy_HDN2_NHWC_biases;
        break;
    case Variant::HDN3:
        this->kptr = param::ACNetLegacy_HDN3_NHWC_kernels;
        this->bptr = param::ACNetLegacy_HDN3_NHWC_biases;
        break;
    }
}

template<int F>
ac::core::model::ACNet<F>::ACNet(const Variant v) noexcept
{
    if constexpr (F == 8)
    {
        switch (v)
        {
        case Variant::B4_NORMAL:
            this->blockNum = 4;
            this->kptr = param::ACNet_F8B4_NHWC_kernels;
            this->bptr = param::ACNet_F8B4_NHWC_biases;
            this->aptr = param::ACNet_F8B4_NHWC_alphas;
            break;
        case Variant::B4_HDN:
            this->blockNum = 4;
            this->kptr = param::ACNet_F8B4_HDN_NHWC_kernels;
            this->bptr = param::ACNet_F8B4_HDN_NHWC_biases;
            this->aptr = param::ACNet_F8B4_HDN_NHWC_alphas;
            break;
        case Variant::B4_BOX:
            this->blockNum = 4;
            this->kptr = param::ACNet_F8B4_Box_NHWC_kernels;
            this->bptr = param::ACNet_F8B4_Box_NHWC_biases;
            this->aptr = param::ACNet_F8B4_Box_NHWC_alphas;
            break;
        case Variant::B4_BOX_HDN:
            this->blockNum = 4;
            this->kptr = param::ACNet_F8B4_Box_HDN_NHWC_kernels;
            this->bptr = param::ACNet_F8B4_Box_HDN_NHWC_biases;
            this->aptr = param::ACNet_F8B4_Box_HDN_NHWC_alphas;
            break;
        case Variant::B8_NORMAL:
            this->blockNum = 8;
            this->kptr = param::ACNet_F8B8_NHWC_kernels;
            this->bptr = param::ACNet_F8B8_NHWC_biases;
            this->aptr = param::ACNet_F8B8_NHWC_alphas;
            break;
        case Variant::B8_HDN:
            this->blockNum = 8;
            this->kptr = param::ACNet_F8B8_HDN_NHWC_kernels;
            this->bptr = param::ACNet_F8B8_HDN_NHWC_biases;
            this->aptr = param::ACNet_F8B8_HDN_NHWC_alphas;
            break;
        case Variant::B8_BOX:
            this->blockNum = 8;
            this->kptr = param::ACNet_F8B8_Box_NHWC_kernels;
            this->bptr = param::ACNet_F8B8_Box_NHWC_biases;
            this->aptr = param::ACNet_F8B8_Box_NHWC_alphas;
            break;
        case Variant::B8_BOX_HDN:
            this->blockNum = 8;
            this->kptr = param::ACNet_F8B8_Box_HDN_NHWC_kernels;
            this->bptr = param::ACNet_F8B8_Box_HDN_NHWC_biases;
            this->aptr = param::ACNet_F8B8_Box_HDN_NHWC_alphas;
            break;
        case Variant::B18_NORMAL:
            this->blockNum = 18;
            this->kptr = param::ACNet_F8B18_NHWC_kernels;
            this->bptr = param::ACNet_F8B18_NHWC_biases;
            this->aptr = param::ACNet_F8B18_NHWC_alphas;
            break;
        case Variant::B18_HDN:
            this->blockNum = 18;
            this->kptr = param::ACNet_F8B18_HDN_NHWC_kernels;
            this->bptr = param::ACNet_F8B18_HDN_NHWC_biases;
            this->aptr = param::ACNet_F8B18_HDN_NHWC_alphas;
            break;
        case Variant::B18_BOX:
            this->blockNum = 18;
            this->kptr = param::ACNet_F8B18_Box_NHWC_kernels;
            this->bptr = param::ACNet_F8B18_Box_NHWC_biases;
            this->aptr = param::ACNet_F8B18_Box_NHWC_alphas;
            break;
        case Variant::B18_BOX_HDN:
            this->blockNum = 18;
            this->kptr = param::ACNet_F8B18_Box_HDN_NHWC_kernels;
            this->bptr = param::ACNet_F8B18_Box_HDN_NHWC_biases;
            this->aptr = param::ACNet_F8B18_Box_HDN_NHWC_alphas;
            break;
        }
    }
    else static_assert(F == 8, "Unsupported ACNet model");
}

template class ac::core::model::ACNet<8>;

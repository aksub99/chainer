#include "xchainer/gradient_check.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <gsl/gsl>

#include "xchainer/array.h"
#include "xchainer/array_repr.h"
#include "xchainer/device_id.h"
#include "xchainer/native_backend.h"
#include "xchainer/shape.h"

namespace xchainer {
namespace {

class NumericalGradientTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        backend_ = std::make_unique<NativeBackend>();
        device_scope_ = std::make_unique<DeviceScope>(backend_.get());
    }

    virtual void TearDown() {
        device_scope_.reset();
        backend_.reset();
    }

public:
    using Arrays = std::vector<Array>;

    template <typename T>
    Array MakeArray(std::initializer_list<int64_t> shape, std::initializer_list<T> data) {
        auto a = std::make_unique<T[]>(data.size());
        std::copy(data.begin(), data.end(), a.get());
        return Array::FromBuffer(shape, TypeToDtype<T>, std::move(a));
    }

    template <typename T>
    Array MakeArray(const Shape& shape, const T* data) {
        int64_t size = shape.GetTotalSize();
        auto a = std::make_unique<T[]>(size);
        std::copy(data, data + size, a.get());
        return Array::FromBuffer(shape, TypeToDtype<T>, std::move(a));
    }

    template <typename T>
    void CheckElementwiseNumericalGradient(const std::function<Arrays(const Arrays&)>& func, const Arrays& center_inputs,
                                           const Arrays& grad_outputs, const Arrays& eps, const Arrays& expected_grads) {
        size_t nin = center_inputs.size();

        auto checked_func = [&](const Arrays& inputs) -> Arrays {
            EXPECT_EQ(inputs.size(), nin) << "The number of inputs given to the function is wrong";
            for (size_t i = 0; i < center_inputs.size(); ++i) {
                EXPECT_EQ(inputs[i].shape(), center_inputs[i].shape()) << "Shape of inputs given to the function is wrong";
                EXPECT_EQ(inputs[i].dtype(), center_inputs[i].dtype()) << "Dtype of inputs given to the function is wrong";
            }

            return func(inputs);
        };

        Arrays grads = CalculateNumericalGradient(checked_func, center_inputs, grad_outputs, eps);

        EXPECT_EQ(grads.size(), expected_grads.size());
        for (size_t i = 0; i < nin; ++i) {
            auto grads_data = static_cast<const T*>(grads[i].data().get());
            auto expected_grads_data = static_cast<const T*>(expected_grads.at(i).data().get());

            int64_t total_size = grads.at(i).GetTotalSize();
            for (int64_t i = 0; i < total_size; ++i) {
                EXPECT_NEAR(grads_data[i], expected_grads_data[i], 1e-3f) << "gradient mismatch at i=" << i;
            }
        }
    }

private:
    std::unique_ptr<Backend> backend_;
    std::unique_ptr<DeviceScope> device_scope_;
};

TEST_F(NumericalGradientTest, NumericalGradientAdd) {
    Shape shape{2, 3};
    float data1[]{1.f, 2.f, -3.f, 4.f, 0.5f, 3.f};
    float data2[]{0.f, 1.3f, 2.f, 3.f, -0.5f, 3.f};
    float eps1[]{1e-3f, -1e-3f, 1e-3f, 1e-3f, -1e-3f, 1e-3f};
    float eps2[]{1e-3f, 1e-3f, -1e-3f, 1e-3f, 1e-3f, 1e-3f};
    float grad_output_data[]{1.f, -2.f, 3.f, 0.f, 3.2f, -1.f};

    Arrays inputs = {
        MakeArray(shape, data1), MakeArray(shape, data2),
    };
    Arrays eps = {
        MakeArray(shape, eps1), MakeArray(shape, eps2),
    };
    Arrays grad_outputs = {
        MakeArray(shape, grad_output_data),
    };

    // Forward function
    auto forward = [](const Arrays& inputs) { return Arrays{inputs[0] + inputs[1]}; };

    // Create expected gradients
    Arrays expected_grads = {grad_outputs[0], grad_outputs[0]};

    // Check
    CheckElementwiseNumericalGradient<float>(forward, inputs, grad_outputs, eps, expected_grads);
}

TEST_F(NumericalGradientTest, NumericalGradientMul) {
    Shape shape{2, 3};
    float data1[]{1.f, 2.f, 3.f, 4.f, -2.f, -3.f};
    float data2[]{0.f, 1.f, 2.f, 3.f, 2.f, 3.f};
    float eps1[]{1e-3f, -1e-3f, 1e-3f, 1e-3f, -1e-3f, 1e-3f};
    float eps2[]{1e-3f, 1e-3f, -1e-3f, 1e-3f, 1e-3f, 1e-3f};
    float grad_output_data[]{1.f, -2.f, 3.f, 0.f, 2.2f, 1.f};

    Arrays inputs = {
        MakeArray(shape, data1), MakeArray(shape, data2),
    };
    Arrays eps = {
        MakeArray(shape, eps1), MakeArray(shape, eps2),
    };
    Arrays grad_outputs = {
        MakeArray(shape, grad_output_data),
    };

    // Forward function
    auto forward = [](const Arrays& inputs) { return Arrays{inputs[0] * inputs[1]}; };

    // Create expected gradients
    Arrays expected_grads = {inputs[1] * grad_outputs[0], inputs[0] * grad_outputs[0]};

    // Check
    CheckElementwiseNumericalGradient<float>(forward, inputs, grad_outputs, eps, expected_grads);
}

}  // namespace
}  // namespace xchainer

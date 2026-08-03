#include "platform/single_instance.hpp"
#include "platform/single_instance_name.hpp"

#include <sddl.h>
#include <windows.h>

#include <string>

namespace squiflow::platform {
namespace {

std::wstring widen_ascii(const std::string& value) {
    return {value.begin(), value.end()};
}

class SharedKernelSecurity final {
public:
    SharedKernelSecurity() {
        // The database is machine-wide, so every authenticated counter account
        // must observe the same mutex and may request activation. The names do
        // not expose the data path and grant no filesystem access.
        constexpr wchar_t descriptor[] =
            L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;AU)";
        if (ConvertStringSecurityDescriptorToSecurityDescriptorW(
                descriptor, SDDL_REVISION_1, &descriptor_, nullptr) != 0) {
            attributes_.nLength = sizeof(attributes_);
            attributes_.lpSecurityDescriptor = descriptor_;
            attributes_.bInheritHandle = FALSE;
            valid_ = true;
        }
    }

    ~SharedKernelSecurity() {
        if (descriptor_ != nullptr) {
            LocalFree(descriptor_);
        }
    }

    SharedKernelSecurity(const SharedKernelSecurity&) = delete;
    SharedKernelSecurity& operator=(const SharedKernelSecurity&) = delete;

    bool valid() const noexcept { return valid_; }
    SECURITY_ATTRIBUTES* attributes() noexcept { return &attributes_; }

private:
    PSECURITY_DESCRIPTOR descriptor_{nullptr};
    SECURITY_ATTRIBUTES attributes_{};
    bool valid_{false};
};

class WindowsSingleInstanceLock final : public SingleInstanceLock {
public:
    ~WindowsSingleInstanceLock() override { release(); }

    InstanceAcquireResult acquire(const std::string& application_id,
                                  const std::string& data_directory) override {
        if (state_ != InstanceState::Idle) {
            return fail(InstanceFault::LockWaitFailed,
                        "The lock object is already in use.");
        }

        const SingleInstanceNames names =
            make_single_instance_names(application_id, data_directory);
        if (!names.ok) {
            return fail(InstanceFault::InvalidDataDirectory, names.error);
        }

        SharedKernelSecurity security;
        if (!security.valid()) {
            return fail(InstanceFault::PermissionDenied,
                        "Cannot prepare machine-wide lock permissions.");
        }

        event_ = CreateEventW(security.attributes(), FALSE, FALSE,
                              widen_ascii(names.activation_name).c_str());
        if (event_ == nullptr) {
            return fail(InstanceFault::ActivationCreateFailed,
                        "Cannot create the activation event.");
        }

        mutex_ = CreateMutexW(security.attributes(), FALSE,
                              widen_ascii(names.mutex_name).c_str());
        if (mutex_ == nullptr) {
            close_handles();
            return fail(InstanceFault::LockCreateFailed,
                        "Cannot create the named mutex.");
        }

        const DWORD wait_result = WaitForSingleObject(mutex_, 0);
        if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED) {
            owns_mutex_ = true;
            state_ = InstanceState::Primary;
            return {state_, InstanceFault::None, "Primary instance acquired.",
                    wait_result == WAIT_ABANDONED};
        }

        if (wait_result == WAIT_TIMEOUT) {
            if (SetEvent(event_) == 0) {
                close_handles();
                return fail(InstanceFault::ActivationSignalFailed,
                            "Cannot signal the primary instance.");
            }
            state_ = InstanceState::Secondary;
            return {state_, InstanceFault::None,
                    "Existing instance notified.", false};
        }

        close_handles();
        return fail(InstanceFault::LockWaitFailed,
                    "Cannot wait for the named mutex.");
    }

    bool take_activation_request() noexcept override {
        return state_ == InstanceState::Primary && event_ != nullptr
               && WaitForSingleObject(event_, 0) == WAIT_OBJECT_0;
    }

    InstanceState state() const noexcept override { return state_; }

    void release() noexcept override {
        if (owns_mutex_ && mutex_ != nullptr) {
            (void)ReleaseMutex(mutex_);
        }
        owns_mutex_ = false;
        close_handles();
        state_ = InstanceState::Idle;
    }

private:
    InstanceAcquireResult fail(InstanceFault fault, const std::string& message) {
        state_ = InstanceState::Failed;
        return {state_, fault, message, false};
    }

    void close_handles() noexcept {
        if (mutex_ != nullptr) {
            CloseHandle(mutex_);
            mutex_ = nullptr;
        }
        if (event_ != nullptr) {
            CloseHandle(event_);
            event_ = nullptr;
        }
    }

    HANDLE mutex_{nullptr};
    HANDLE event_{nullptr};
    bool owns_mutex_{false};
    InstanceState state_{InstanceState::Idle};
};

}  // namespace

std::unique_ptr<SingleInstanceLock> make_single_instance_lock() {
    return std::make_unique<WindowsSingleInstanceLock>();
}

}  // namespace squiflow::platform

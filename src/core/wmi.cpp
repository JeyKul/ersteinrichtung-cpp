#include "wmi.hpp"

#include <windows.h>
#include <combaseapi.h>
#include <oleauto.h>
#include <rpc.h>
#include <wbemcli.h>

namespace core {
namespace {

// Runs `SELECT <property> FROM <wmiClass>` and fills `out` with the property
// value of the first returned instance.
bool queryFirstInstance(
    const std::wstring& wmiClass,
    const std::wstring& property,
    VARIANT* out) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize =
        initialized == S_OK || initialized == RPC_E_CHANGED_MODE;

    IWbemLocator* locator = nullptr;
    const HRESULT created = CoCreateInstance(
        CLSID_WbemLocator,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        reinterpret_cast<void**>(&locator));

    bool succeeded = false;

    if (created == S_OK && locator != nullptr) {
        IWbemServices* services = nullptr;

        // ConnectServer takes `const BSTR` (a non-const pointee), so pass a
        // mutable wide buffer rather than a string literal.
        std::wstring networkResource = L"root\\CIMV2";
        const HRESULT connected = locator->ConnectServer(
            networkResource.data(),
            nullptr,
            nullptr,
            nullptr,
            0,
            nullptr,
            nullptr,
            &services);

        if (connected == S_OK && services != nullptr) {
            CoSetProxyBlanket(
                services,
                RPC_C_AUTHN_WINNT,
                RPC_C_AUTHZ_NONE,
                nullptr,
                RPC_C_AUTHN_LEVEL_CALL,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr,
                EOAC_NONE);

            std::wstring queryLanguage = L"WMI";
            std::wstring query =
                L"SELECT " + property + L" FROM " + wmiClass;

            IEnumWbemClassObject* enumerator = nullptr;
            // Synchronous query (flag 0): ExecQuery blocks until the result set
            // is ready, so Next() reliably waits for the first row.
            const HRESULT queried = services->ExecQuery(
                queryLanguage.data(),
                query.data(),
                0,
                nullptr,
                &enumerator);

            if (queried == S_OK && enumerator != nullptr) {
                IWbemClassObject* object = nullptr;

                if (enumerator->Next(WBEM_INFINITE, 1, &object, nullptr) ==
                    S_OK) {
                    std::wstring propertyName = property;
                    if (object->Get(propertyName.data(), 0, out, nullptr, nullptr) ==
                        S_OK) {
                        succeeded = true;
                    }

                    object->Release();
                }

                enumerator->Release();
            }

            services->Release();
        }

        locator->Release();
    }

    if (shouldUninitialize) {
        CoUninitialize();
    }
    return succeeded;
}

std::wstring trim(const std::wstring& value) {
    const std::size_t begin = value.find_first_not_of(L" \t");
    if (begin == std::wstring::npos) {
        return {};
    }

    const std::size_t end = value.find_last_not_of(L" \t");
    return value.substr(begin, end - begin + 1);
}

}  // namespace

std::optional<std::wstring> queryWmiStringProperty(
    const std::wstring& wmiClass,
    const std::wstring& property) {
    VARIANT value;
    VariantInit(&value);

    if (queryFirstInstance(wmiClass, property, &value)) {
        if (value.vt == VT_BSTR && value.bstrVal != nullptr) {
            const std::wstring text = value.bstrVal;
            VariantClear(&value);
            return trim(text);
        }
    }

    VariantClear(&value);
    return std::nullopt;
}

std::optional<std::uint32_t> queryWmiUintProperty(
    const std::wstring& wmiClass,
    const std::wstring& property) {
    VARIANT value;
    VariantInit(&value);

    std::optional<std::uint32_t> result = std::nullopt;

    if (queryFirstInstance(wmiClass, property, &value)) {
        if (value.vt == VT_UI4) {
            result = value.uiVal;
        } else if (value.vt == VT_I4 && value.lVal >= 0) {
            result = static_cast<std::uint32_t>(value.lVal);
        }
    }

    VariantClear(&value);
    return result;
}

}  // namespace core

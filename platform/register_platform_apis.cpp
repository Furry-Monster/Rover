#include "platform/register_platform_apis.h"

#if defined(ROVER_PLATFORM_LINUX)
    #include "platform/linux/register_types.h"
#elif defined(ROVER_PLATFORM_WINDOWS)
    #include "platform/windows/register_types.h"
#elif defined(ROVER_PLATFORM_MAC)
    #include "platform/mac/register_types.h"
#elif defined(ROVER_PLATFORM_ANDROID)
    #include "platform/android/register_types.h"
#elif defined(ROVER_PLATFORM_IOS)
    #include "platform/ios/register_types.h"
#elif defined(ROVER_PLATFORM_WEB)
    #include "platform/web/register_types.h"
#endif

namespace rover
{

    void register_platform_apis()
    {
#if defined(ROVER_PLATFORM_LINUX)
        register_linux_platform();
#elif defined(ROVER_PLATFORM_WINDOWS)
        register_windows_platform();
#elif defined(ROVER_PLATFORM_MAC)
        register_mac_platform();
#elif defined(ROVER_PLATFORM_ANDROID)
        register_android_platform();
#elif defined(ROVER_PLATFORM_IOS)
        register_ios_platform();
#elif defined(ROVER_PLATFORM_WEB)
        register_web_platform();
#endif
    }

    void unregister_platform_apis()
    {
#if defined(ROVER_PLATFORM_LINUX)
        unregister_linux_platform();
#elif defined(ROVER_PLATFORM_WINDOWS)
        unregister_windows_platform();
#elif defined(ROVER_PLATFORM_MAC)
        unregister_mac_platform();
#elif defined(ROVER_PLATFORM_ANDROID)
        unregister_android_platform();
#elif defined(ROVER_PLATFORM_IOS)
        unregister_ios_platform();
#elif defined(ROVER_PLATFORM_WEB)
        unregister_web_platform();
#endif
    }

} // namespace rover

#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include "main_menu.hpp"
#include "language.hpp"
#include "refresh.hpp"

// KeyX 特斯拉覆盖层主类
class KeyXOverlay : public tsl::Overlay {

private:

    bool m_isFirstShow = true;          // 是否是第一次显示

public:
    // 初始化系统服务
    virtual void initServices() override 
    {            
        pmdmntInitialize();                                           // 进程管理服务
        setInitialize();                                              // 初始化set服务（获取系统语言）
        nsInitialize();                                               // 初始化ns服务
        InitSystemLanguage();                                         // 获取系统语言（用于显示游戏名）
        setExit();                                                    // 退出set服务

        // pdmqry服务会话数量限制，用这个方法可以绕开限制，以防和别的插件或者游戏发生冲突引发崩溃
        Result rc = pdmqryInitialize();
        if (R_SUCCEEDED(rc)) {
            Service* pdmqrySrv = pdmqryGetServiceSession();
            Service pdmqryClone;
            serviceClone(pdmqrySrv, &pdmqryClone);
            serviceClose(pdmqrySrv);
            memcpy(pdmqrySrv, &pdmqryClone, sizeof(Service));
        }
    }
    
    // 退出系统服务
    virtual void exitServices() override 
    {
        nsExit();                   // 退出 ns 服务
        pdmqryExit();               // 退出 pdmqry 服务
        pmdmntExit();               // 退出进程管理服务
    }

    // 覆盖层显示时调用，用于更新游戏信息
    virtual void onShow() override {
        if (m_isFirstShow) m_isFirstShow = false;
        else Refresh::RefrRequest(Refresh::OnShow);
    }

    // 加载初始GUI界面
    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainMenu>();  // 返回主菜单界面
    }
};

// 程序入口点
int main(int argc, char **argv) {
    return tsl::loop<KeyXOverlay>(argc, argv);
}

#include "ipc.hpp"
#include <cstring>

// 静态线程栈定义
alignas(0x1000) char IPCServer::ipc_thread_stack[8 * 1024];

// 构造函数
IPCServer::IPCServer() : 
    m_IsClientConnected(false),
    m_ShouldExit(false),
    m_ThreadCreated(false),
    m_ThreadRunning(false) {
    
    m_Handles[0] = INVALID_HANDLE;
    m_Handles[1] = INVALID_HANDLE;
    memset(&m_ServerName, 0, sizeof(SmServiceName));
    memset(&m_IpcThread, 0, sizeof(Thread));
}

// 析构函数
IPCServer::~IPCServer() {
    Stop();
}

// 启动IPC服务
bool IPCServer::Start(const char* service_name) {
    memset(&m_ServerName, 0, sizeof(SmServiceName));
    memcpy(m_ServerName.name, service_name, 
           service_name[7] == '\0' ? 8 : 7);
    
    Result rc = threadCreate(&m_IpcThread, ThreadEntry, this, 
                           ipc_thread_stack, sizeof(ipc_thread_stack), 44, -2);
    if (R_FAILED(rc)) return false;
    m_ThreadCreated = true;
    
    rc = threadStart(&m_IpcThread);
    if (R_FAILED(rc)) return false;
    m_ThreadRunning = true;
    return true;
}

// 停止IPC服务
void IPCServer::Stop() {
    if (!m_ThreadCreated) return;
    m_ShouldExit = true;
    if (m_ThreadRunning) threadWaitForExit(&m_IpcThread);
    if (m_ThreadCreated) threadClose(&m_IpcThread);
}

void IPCServer::SetExitCallback(std::function<void()> callback) {
    m_ExitCallback = callback;
}

void IPCServer::SetEnableAutoFireCallback(std::function<void()> callback) {
    m_EnableAutoFireCallback = callback;
}

void IPCServer::SetDisableAutoFireCallback(std::function<void()> callback) {
    m_DisableAutoFireCallback = callback;
}

void IPCServer::SetReloadAutoFireCallback(std::function<void()> callback) {
    m_ReloadAutoFireCallback = callback;
}

// 静态线程入口函数
void IPCServer::ThreadEntry(void* arg) {
    IPCServer* server = static_cast<IPCServer*>(arg);
    server->IpcThreadMain();
}

// 线程主循环
void IPCServer::IpcThreadMain() {
    StartServer();
    while (!m_ShouldExit) {
        WaitAndProcessRequest();
    }
    StopServer();
}

// 启动服务器
void IPCServer::StartServer() {
    Result rc = smRegisterService(m_ServerHandle, m_ServerName, false, 1);
    if (R_FAILED(rc)) {
        m_ShouldExit = true;
        return;
    }
}

// 停止服务器
void IPCServer::StopServer() {
    if (m_IsClientConnected && *m_ClientHandle != INVALID_HANDLE) {
        svcCloseHandle(*m_ClientHandle);
        *m_ClientHandle = INVALID_HANDLE;
        m_IsClientConnected = false;
    }
    if (*m_ServerHandle != INVALID_HANDLE) {
        svcCloseHandle(*m_ServerHandle);
        smUnregisterService(m_ServerName);
        *m_ServerHandle = INVALID_HANDLE;
    }
}

// 等待并处理请求
void IPCServer::WaitAndProcessRequest() {
    s32 index = -1;
    
    Result rc = svcWaitSynchronization(&index, m_Handles, m_IsClientConnected ? 2 : 1, UINT64_MAX);
    if (R_FAILED(rc)) {
        m_ShouldExit = true;
        return;
    }
    
    if (index == 0) {
        Handle new_client;
        rc = svcAcceptSession(&new_client, *m_ServerHandle);
        if (R_FAILED(rc)) return;
        
        if (m_IsClientConnected) {
            svcCloseHandle(new_client);
            return;
        }
        
        m_IsClientConnected = true;
        *m_ClientHandle = new_client;
        
    } else if (index == 1) {
        if (!m_IsClientConnected) {
            m_ShouldExit = true;
            return;
        }
        
        s32 _idx;
        rc = svcReplyAndReceive(&_idx, m_ClientHandle, 1, 0, UINT64_MAX);
        if (R_FAILED(rc)) return;
        
        bool should_close = false;
        CommandResult cmd_result = {false, false, false, false, false};
        Request request = ParseRequestFromTLS();
        
        switch (request.type) {
            case CmifCommandType_Request:
                cmd_result = HandleCommand(request.cmd_id);
                should_close = cmd_result.should_close_connection;
                break;
            case CmifCommandType_Close:
                WriteResponseToTLS(0);
                should_close = true;
                break;
            default:
                WriteResponseToTLS(1);
                break;
        }
        
        // 先发送响应（此时服务器处于正常运行状态）
        rc = svcReplyAndReceive(&_idx, m_ClientHandle, 0, *m_ClientHandle, 0);
        
        if (should_close) {
            svcCloseHandle(*m_ClientHandle);
            *m_ClientHandle = INVALID_HANDLE;
            m_IsClientConnected = false;
        }
        
        // 最后才执行回调逻辑（确保响应已成功发送）
        if (cmd_result.should_enable_autofire) {
            if (m_EnableAutoFireCallback) m_EnableAutoFireCallback();
        }
        if (cmd_result.should_disable_autofire) {
            if (m_DisableAutoFireCallback) m_DisableAutoFireCallback();
        }
        if (cmd_result.should_reload_autofire) {
            if (m_ReloadAutoFireCallback) m_ReloadAutoFireCallback();
        }
        if (cmd_result.should_exit_server) {
            m_ShouldExit = true;
            if (m_ExitCallback) m_ExitCallback();
        }
    }
}

// 处理命令
CommandResult IPCServer::HandleCommand(u64 cmd_id) {
    CommandResult result = {false, false, false, false, false};
    
    switch (cmd_id) {
        case CMD_ENABLE_AUTOFIRE:
            WriteResponseToTLS(0);
            result.should_enable_autofire = true;
            break;
            
        case CMD_DISABLE_AUTOFIRE:
            WriteResponseToTLS(0);
            result.should_disable_autofire = true;
            break;
            
        case CMD_RELOAD_AUTOFIRE:
            WriteResponseToTLS(0);
            result.should_reload_autofire = true;
            break;
            
        case CMD_EXIT:
            WriteResponseToTLS(0);
            result.should_close_connection = true;
            result.should_exit_server = true;
            break;
            
        default:
            WriteResponseToTLS(1);
            break;
    }
    
    return result;
}

// 解析TLS中的请求
IPCServer::Request IPCServer::ParseRequestFromTLS() {
    Request req = {0};
    
    void* base = armGetTls();
    HipcParsedRequest hipc = hipcParseRequest(base);
    
    req.type = hipc.meta.type;
    
    if (hipc.meta.type == CmifCommandType_Request) {
        CmifInHeader* header = (CmifInHeader*)cmifGetAlignedDataStart(hipc.data.data_words, base);
        size_t data_size = hipc.meta.num_data_words * 4;
        
        if (!header) return req;
        if (data_size < sizeof(CmifInHeader)) return req;
        if (header->magic != CMIF_IN_HEADER_MAGIC) return req;
        
        req.cmd_id = header->command_id;
        req.data_size = data_size - sizeof(CmifInHeader);
        req.data = req.data_size ? ((u8*)header) + sizeof(CmifInHeader) : NULL;
    }
    
    return req;
}

// 写响应到TLS
void IPCServer::WriteResponseToTLS(Result rc) {
    HipcMetadata meta = {0};
    meta.type = CmifCommandType_Request;
    meta.num_data_words = (sizeof(CmifOutHeader) + 0x10) / 4;
    
    void* base = armGetTls();
    HipcRequest hipc = hipcMakeRequest(base, meta);
    CmifOutHeader* raw_header = (CmifOutHeader*)cmifGetAlignedDataStart(hipc.data_words, base);
    
    raw_header->magic = CMIF_OUT_HEADER_MAGIC;
    raw_header->result = rc;
    raw_header->token = 0;
}

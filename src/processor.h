/*
 * Tencent is pleased to support the open source community by making Pebble available.
 * Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.
 * Licensed under the MIT License (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 *
 */


#ifndef _PEBBLE_PROCESSOR_H_
#define _PEBBLE_PROCESSOR_H_

#include "error.h"
#include "platform.h"


namespace pebble {

/* @brief Processor错误码定义 */
typedef enum {
    kPROCESSOR_ERROR_BASE              = PROCESSOR_ERROR_CODE_BASE,
    kPROCESSOR_INVALID_PARAM           = kPROCESSOR_ERROR_BASE - 1,   /* 参数错误 */
    kPROCESSOR_EMPTY_SEND              = kPROCESSOR_ERROR_BASE - 2,   /* send函数未设置 */
} ProcessorErrorCode;

class ProcessorErrorStringRegister {
public:
    static void RegisterErrorString() {
        SetErrorString(kPROCESSOR_INVALID_PARAM, "invalid paramater");
        SetErrorString(kPROCESSOR_EMPTY_SEND, "not set send or sendv function");
    }
};

/*
    @brief send函数定义
    @param flag message接口可选参数，默认为0
*/
typedef cxx::function<
    int32_t(int64_t handle, const uint8_t* buff, uint32_t buff_len, int32_t flag)> SendFunction;

/*
    @brief sendv函数定义
    @param flag message接口可选参数，默认为0
*/
typedef cxx::function<
    int32_t(int64_t handle, uint32_t msg_frag_num,
    const uint8_t* msg_frag[], uint32_t msg_frag_len[], int32_t flag)> SendVFunction;

/*
    @brief IEventHandler是对Processor通用处理的抽象，和Processor配套使用
        一般建议Processor本身处理和协议、业务强相关的逻辑，针对消息的一些通用处理由EventHandler完成
*/
class IEventHandler {
public:
    virtual ~IEventHandler() {}

    /*
        @brief 上报传输质量，依据请求结果和耗时来判断目的质量
        @param handle 传输句柄
        @param ret_code 请求处理结果
        @param time_cost_ms 处理时间
    */
    virtual void ReportTransportQuality(int64_t handle, int32_t ret_code,
        int64_t time_cost_ms) = 0;

    /*
        @brief 请求处理完成事件，在请求消息处理完成时被回调
        @param name 消息名称
        @param result 处理结果
        @param time_cost_ms 处理耗时，单位毫秒
        @note 多维度的数据由用户自己组装到name中，最终落地到文件，计算与展示由业务的数据分析系统完成
    */
    virtual void RequestProcComplete(const std::string& name,
        int32_t result, int32_t time_cost_ms) = 0;

    /*
        @brief 响应处理完成事件，在响应消息处理完成时被回调
        @param name 消息名称
        @param result 请求结果
        @param time_cost_ms 请求耗时，单位毫秒
        @note 多维度的数据由用户自己组装到name中，最终落地到文件，计算与展示由业务的数据分析系统完成
    */
    virtual void ResponseProcComplete(const std::string& name,
        int32_t result, int32_t time_cost_ms) = 0;

    /*
        @brief 向统计模块添加一个名字，在一个统计周期内未处理消息默认上报0
        @note 如果一个接口在统计周期内没处理消息，但是又需要上报数据，需要显式的注册给统计模块
    */
    virtual void AddNameToStat(const std::string& name) = 0;

    /* @brief 从统计模块剔除一个名字，在一个统计周期内未处理消息则不上报任何数据 */
    virtual void RemoveNameFromStat(const std::string& name) = 0;
};

/* @brief Pebble消息处理抽象接口 */
class IProcessor {
public:
    IProcessor();
    virtual ~IProcessor();

    /*
        @brief 设置send函数，processor使用设置的send函数发送消息
        @param send  函数原型为int32_t(int64_t, const uint8_t*, uint32_t, int32_t)
        @param sendv 函数原型为int32_t(int64_t, uint32_t, const uint8_t*[], uint32_t[], int32_t)
        @return 0 成功
        @return 非0 失败
    */
    virtual int32_t SetSendFunction(const SendFunction& send, const SendVFunction& sendv);

    /*
        @brief 设置EventHandler，在processor中调用EventHandler接口以实现消息通用功能
        @param event_handler IEventHandler接口实例，不同类型的processor上有不同的EventHandler实现
        @return 0 成功
        @return 非0 失败
    */
    virtual int32_t SetEventHandler(IEventHandler* event_handler);

    /*
        @brief 消息处理入口(Processor的输入)
        @param handle 网络句柄(用于传输消息的连接或消息队列标识)
        @param msg 消息
        @param msg_len 消息长度
        @param msg_info 消息属性
        @param is_overload 是否过载(框架根据程序运行情况告知Processor目前是否过载，由Processor自行过载处理)
            涵盖了具体的过载类型，具体可参考 @see OverLoadType
        @return 0 成功
        @return 非0 失败
    */
    virtual int32_t OnMessage(int64_t handle, const uint8_t* msg, uint32_t msg_len) = 0;

    /*
        @brief 事件驱动
        @return 处理的事件数，0表示无事件
    */
    virtual int32_t Update() = 0;

    /*
        @brief Processor发送消息接口，实际使用SetSendFunction设置的send函数，用户可扩展在send前做些特殊处理
        @return 0 成功，<0 失败
    */
    virtual int32_t Send(int64_t handle, const uint8_t* msg, uint32_t msg_len, int32_t flag);

    /*
        @brief Processor发送消息接口，实际使用SetSendFunction设置的sendv函数，用户可扩展在send前做些特殊处理
        @return 0 成功，<0 失败
    */
    virtual int32_t SendV(int64_t handle, uint32_t msg_frag_num,
                          const uint8_t* msg_frag[], uint32_t msg_frag_len[], int32_t flag);

    /*
        @brief Processor上报动态资源使用情况，由各Processor实现者填写内部维护的动态资源信息，框架定期调用
            如 Processor内部session的个数 myprocessor:session:9999
        @param resource_info name-value的表，name为资源名称，value为值
        @note Processor的对象标示可以组合在name中
    */
    virtual void GetResourceUsed(cxx::unordered_map<std::string, int64_t>* resource_info) = 0;

protected:
    IEventHandler* m_event_handler;
    SendFunction   m_send;
    SendVFunction  m_sendv;
};

} // namespace pebble

#endif // _PEBBLE_PROCESSOR_H_

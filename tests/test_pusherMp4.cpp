﻿/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Util/NoticeCenter.h"
#include "Poller/EventPoller.h"
#include "Player/PlayerProxy.h"
#include "Rtmp/RtmpPusher.h"
#include "Common/config.h"
#include "Pusher/MediaPusher.h"
#include "MediaFile/MediaReader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

//推流器，保持强引用
MediaPusher::Ptr pusher;

//声明函数
void rePushDelay(const string &schema,const string &vhost,const string &app, const string &stream, const string &url);


//创建推流器并开始推流
void createPusher(const string &schema,const string &vhost,const string &app, const string &stream, const string &url) {
	auto src = MediaReader::onMakeMediaSource(schema,vhost,app,stream);
    if(!src){
        //文件不存在
        WarnL << "MP4 file not exited!";
        return;
    }

	//创建推流器并绑定一个MediaSource
    pusher.reset(new MediaPusher(src));
	//设置推流中断处理逻辑
	pusher->setOnShutdown([schema,vhost,app,stream, url](const SockException &ex) {
		WarnL << "Server connection is closed:" << ex.getErrCode() << " " << ex.what();
        //重新推流
		rePushDelay(schema,vhost,app, stream, url);
	});
	//设置发布结果处理逻辑
	pusher->setOnPublished([schema,vhost,app,stream, url](const SockException &ex) {
		if (ex) {
			WarnL << "Publish fail:" << ex.getErrCode() << " " << ex.what();
			//如果发布失败，就重试
			rePushDelay(schema,vhost,app, stream, url);
		}else {
			InfoL << "Publish success,Please play with player:" << url;
		}
	});
	pusher->publish(url);
}

Timer::Ptr g_timer;
//推流失败或断开延迟2秒后重试推流
void rePushDelay(const string &schema,const string &vhost,const string &app, const string &stream, const string &url) {
	g_timer = std::make_shared<Timer>(2,[schema,vhost,app, stream, url]() {
		InfoL << "Re-Publishing...";
		//重新推流
		createPusher(schema,vhost,app, stream, url);
		//此任务不重复
		return false;
	}, nullptr);
}

//这里才是真正执行main函数，你可以把函数名(domain)改成main，然后就可以输入自定义url了
int domain(const string & filePath,const string & pushUrl){
	//设置退出信号处理函数
	static semaphore sem;
	signal(SIGINT, [](int) { sem.post(); });// 设置退出信号

	//设置日志
	Logger::Instance().add(std::make_shared<ConsoleChannel>());
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    //录像应用名称默认为record
    string appName = mINI::Instance()[Record::kAppName];
    //app必须record，filePath(流id)为相对于httpRoot/record的路径，否则MediaReader会找到不该文件
    //限制app为record是为了防止服务器上的文件被肆意访问
    createPusher(FindField(pushUrl.data(), nullptr,"://"),DEFAULT_VHOST,appName,filePath,pushUrl);

    sem.wait();
	return 0;
}



int main(int argc,char *argv[]){
    //MP4文件需要放置在 httpRoot/record目录下,文件负载必须为h264+aac
    //可以使用test_server生成的mp4文件
    return domain("app/stream/2017-09-30/12-55-38.mp4","rtsp://127.0.0.1/live/rtsp_push");
}





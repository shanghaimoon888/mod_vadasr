# Freeswitch VAD ASR 模块

本代码是基于Freeswitch插件方式的实现语音VAD检测和ASR语音识别功能，语音识别使用的是科大讯飞的实时语音转写接口，需要自行申请appid和appkey。模块采用esl事件方式送出识别结果和录音文件，具体使用方法参考如下说明：
1.	配置文件asr.conf
```
<configuration name="asr.conf" description="Asr Setting">
  <settings>
   <!—科大讯飞asr的appid -->
   <param name="appid" value="123456"/>
   <!—科大讯飞asr的appkey -->
   <param name="appkey" value="1111222233334444"/>
  </settings>
</configuration>
```
2.	App(dialplan示例)

App名称： vad
App参数： [<ACTION ><VAD_VOICE_FRAMES> <VAD_SILINCE_FRAMES> <NS_LEVEL>]
ACTION: start 启动  stop 停止
VAD_VOICE_FRAMES：判定检测到多久的语音包认为是有人说话了，单位ms
VAD_SILINCE_FRAMES：判定检测到多久的静音包认为是静音了，单位ms
NS_LEVEL：降噪级别
```
<extension name="public_extensions">
      <condition field="destination_number" expression="^(10[01][0-9])$">
	    <action application="answer"/>
        <action application="vad" data="start 100 500 2"/>
        <action application="sleep" data="5000"/>
        <action application="vad" data="stop"/>
        <action application="park"/>
      </condition>
</extension>
```
3.	Esl event说明
一共有三个自定义事件：
#### #define VAD_EVENT_START "vad::start"
检测到语音，输出自定义头: Vad-Status:start
#### #define VAD_EVENT_STOP "vad::stop"
检测到静音，输出自定义头: Vad-Status:stop
                        Vad-RecordFile: 录音路径
#### #define VAD_EVENT_ASR "vad::asr"
在检测到静音后，输出语音文本，自定义头：Asr-Text:文本内容，注意文本内容可能为空，表示未识别出。



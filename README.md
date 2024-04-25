# app-audio-loopback

特定アプリの音声を別の出力先に出力する。

## 概要

OBSのアプリケーション音声キャプチャでノイズが入る為、回避策として作ったアプリケーション。

![image](https://github.com/ndekopon/app-audio-loopback/assets/92087784/c8d8788e-a620-4274-98cc-9a9769201a6d)


## 使い方

### ダウンロード

- [[releases](https://github.com/ndekopon/app-audio-loopback/releases)] から自分のPCにあったzipをダウンロードする。

※基本的に**app-audio-loopback-vx.x.x-x64.zip**(x64版)を使用

### 基本

`app-audio-loopback.exe`を起動するとタスクトレイに常駐する。

タスクトレイアイコンの右クリックメニューから、`config`を選択して設定ウィンドウを表示する。  
![image](https://github.com/ndekopon/app-audio-loopback/assets/92087784/7583911b-3a9f-4d9b-9618-def4b06efde2)

※設定ウィンドウ  
![image](https://github.com/ndekopon/app-audio-loopback/assets/92087784/9adb06d2-d051-490b-9b30-6cd71ecf30b5)


#### render device

音声の出力先を選択する。
※OBSでキャプチャをしたい場合は、普段使わない出力先を選んでおく。


#### executable file name

実行ファイル名で対象を限定する場合、`chrome.exe`のように実行ファイル名を選択する。


#### window title

ウィンドウタイトルで対象を限定する場合、`メモ帳`のようにウィンドウタイトルを選択する。  
※ウィンドウタイトルが頻繁に変わるアプリケーションの場合、別の条件で限定する


#### window class

ウィンドウタイトルとは別に設定されるウィンドウのクラス名で対象を限定する場合、`Notepad`のようにウィンドウクラスを選択する。


### 設定反映

設定を反映するためには、`OK`を選択する。


### OBS側の設定

デスクトップ音声のデバイスを、`render device`で選択したデバイスにする。

## 動作環境

- Windows11 または Windows10 21H2以上

## テスト環境

下記環境で検証し、OBSでノイズが乗らないことを確認。

- OS: Windows10 22H2(19045.4170)
- CPU: AMD Ryzen 5 5600X
- SoftWare: OBS Studio 30.1.1

## Q&A

* Q. renderに出力したいデバイスが表示されない
    * A. 出力デバイスはサンプルレートが48000Hzになっていないと恐らく表示されないので、48000Hzに変更する
* Q. キャプチャする音声を増やしたい
    * A. exeと同じフォルダにあるiniファイルを編集し、`MAIN`セクションの`TABS`の数値を大きくして保存する(最大16)

## バグ報告

PC環境を記載の上、issuesかtwitterかdiscordでどうぞ。


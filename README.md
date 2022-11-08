# app-audio-lookback

特定アプリの音声を別の出力先に出力する。

## 概要

OBSのアプリケーション音声キャプチャでノイズが入る為、回避策として作ったアプリケーション。

## 使い方

### 基本

`app-audio-loopback.exe`を起動するとタスクトレイに常駐する。

タスクトレイアイコンの右クリックメニューから、`Render`(出力)を選択。

※OBSでキャプチャをしたい場合は、普段使わない出力先を選んでおく。

![image](https://user-images.githubusercontent.com/92087784/200531009-b6d5f341-d62c-4380-81be-35500fb40345.png)

タスクトレイアイコンの右クリックメニューから、'select application'を選択。

キャプチャしたいアプリケーションの"title"(ウィンドウタイトル)、"window class name"(ウィンドウクラス名)、"exe name"(EXEファイル名)から固定かつ一意になるものを選ぶ。

![image](https://user-images.githubusercontent.com/92087784/200532373-064bfc03-b7ff-45fc-aa1b-d7635d5a97e9.png)

マッチ条件を削除したい場合は、右クリックメニューから削除可能。

![image](https://user-images.githubusercontent.com/92087784/200534024-0393cddf-9678-46af-b6ef-8f23b94484e5.png)

### OBSで

デスクトップ音声のデバイスを、`Render`で選択したデバイスにする。


## テスト環境

下記環境で検証し、OBSでノイズが乗らないことを確認。

- OS: Windows10 21H2(19044.2130)
- CPU: AMD Ryzen 5 5600X
- SoftWare: OBS Studio 28.1.2

## Q&A

* Q. renderに出力したいデバイスが表示されない
    * A. 出力デバイスはサンプルレートが48000Hzになっていないと恐らく表示されないので、48000Hzに変更する

## バグ報告

exeファイルと同じフォルダの中に、"logs"フォルダを作成するとその中にログが出力されるようになるので、
出力されたログと共にissuesかtwitterかdiscordでどうぞ。


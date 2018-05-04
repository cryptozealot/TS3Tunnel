#pragma once

#include <QObject>

#include <opus/opus.h>

//#include <portaudio.h>

#include <QBuffer>
#include <QDataStream>
#include <QHash>
#include <QIODevice>
#include <QTimer>
#include <QUdpSocket>
#include <QtMultimedia/QAudioOutput>

class MainWindow;


class Client : public QObject
{
	Q_OBJECT

public:
	Client(const QHostAddress &serverAddress, quint16 serverPort, const QString &serverPassword, QObject *parent);
	~Client();

	int getDecodedVoicePacketsNb() const;
	int getDecodedVoicePacketsBytesNb() const;
	int getDecodingErrorsNb() const;

	bool setupAudioPlayback();
	bool registerToServer();

	void setVoiceSessionState(quint64 sessionId, bool enabled);


signals:
	void newVoiceSession(quint64 sessionId);


private slots:
	void udpSocket_readyRead();
	void serverPingTimer_timeout();


private:
	struct VoiceSession
	{
		quint64 Id;
		bool Enabled;
	};

	void decodeVoiceDataStream(QDataStream &voiceDataStream, QIODevice *outputDevice, qint64 voicePacketBufferReserveSize);
	bool updateVoiceSessionList(quint64 sessionId);

	static const int SERVER_PING_TIMER_INTERVAL_SEC = 2;
	static const QString PING_STR;

	static const qint64 PLAYBACK_AUDIO_BUFFER_SIZE = 960 * 4;

	static const int OPUS_CHANNEL_COUNT = 1;
	static const std::size_t OPUS_SAMPLE_SIZE = sizeof(opus_int16) * 8;
	static const opus_int32 OPUS_SAMPLE_RATE = 48000;
	static const int OPUS_FRAME_SIZE = 960;


	QHostAddress m_serverAddress;
	quint16 m_serverPort;
	QString m_serverPassword;
	QUdpSocket *m_udpSocket;
	QTimer m_serverPingTimer;
	QAudioOutput *m_audioOutput;
	QIODevice *m_playbackDevice;
	OpusDecoder *m_opusDecoder;
	int m_decodedVoicePacketsNb;
	int m_decodedVoicePacketsBytesNb;
	int m_decodingErrorsNb;
	QHash<quint64, VoiceSession> m_voiceSessions;
	//PaStream *m_paStream;
};
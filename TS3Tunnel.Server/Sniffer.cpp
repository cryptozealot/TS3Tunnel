/*
* MIT License
*
* Copyright (c) 2018 Guillaume Truchot - guillaume.truchot@outlook.com
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
#include "Sniffer.h"

#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <pcap.h>

#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QList>

#include "Server.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>

void packetSniffed(u_char *userData, const struct pcap_pkthdr *pcapHeader, const u_char *packet)
{
    Sniffer::SnifferData *snifferData = reinterpret_cast<Sniffer::SnifferData*>(userData);
    QList<Server::ClientInfo> *clients = reinterpret_cast<QList<Server::ClientInfo>*>(snifferData->Clients);
    const ether_header *ethernetHeader = (ether_header*)packet;
    const ip *ipHeader = (const ip*)((const u_char*)ethernetHeader + sizeof(ether_header));
    const udphdr *udpHeader = (const udphdr*)((const u_char*)ipHeader + sizeof(ip));
    struct ether_header* eptr;
    eptr = (struct ether_header*)packet;
    struct ip* ip2;
    ip2 = (struct ip*)(packet + sizeof(struct ether_header));
    const u_char *payload = (const u_char*)udpHeader + sizeof(udphdr);
    const u_char *voiceData = payload + sizeof(Sniffer::Ts3VoicePacketHeader);
    int32_t ethernetPaddingLength = pcapHeader->len - htons(ipHeader->ip_len) - sizeof(ether_header);
    int32_t payloadLength = pcapHeader->len - sizeof(ether_header) - sizeof(ip) - sizeof(udphdr) - ethernetPaddingLength;
    int32_t voiceDataLength = payloadLength >= sizeof(Sniffer::Ts3VoicePacketHeader) ? payloadLength - sizeof(Sniffer::Ts3VoicePacketHeader) : 0;

    if (payloadLength >= sizeof(Sniffer::Ts3VoicePacketHeader))
    {
        const Sniffer::Ts3VoicePacketHeader *voiceHeader = reinterpret_cast<const Sniffer::Ts3VoicePacketHeader*>(payload);

        if (voiceHeader->PacketType == Sniffer::Ts3VoicePacketType::Voice1 ||
                voiceHeader->PacketType == Sniffer::Ts3VoicePacketType::Voice2)
        {
            snifferData->Nb += 1;

            printf("Source IP: %s\n", inet_ntoa(ip2->ip_src));
            printf("%" PRIu64 "\n", voiceHeader->SessionId);

            snifferData->Mutex->lock();
            for (auto it = clients->begin(); it != clients->end(); ++it)
            {
                QByteArray voiceDataBuffer{};
                QDataStream voiceDataStream{ &voiceDataBuffer, QIODevice::WriteOnly };

                voiceDataStream << static_cast<quint16>(voiceDataLength);
                voiceDataStream << static_cast<quint64>(voiceHeader->SessionId);
                voiceDataStream.writeRawData(reinterpret_cast<const char*>(voiceData), voiceDataLength);

                snifferData->Socket->writeDatagram(voiceDataBuffer, it->Address, it->Port);
            }
            snifferData->Mutex->unlock();
        }
    }
}


Sniffer::Sniffer(const QString &interfaceName, const QString &ts3VoicePort, QUdpSocket *socket, QList<Server::ClientInfo> *clients, QMutex *mutex, QObject *parent) : QObject{ parent },
    m_interfaceName{ interfaceName },
    m_ts3VoicePort{ ts3VoicePort },
    m_snifferData{}
{
    m_snifferData.Nb = 0;
    m_snifferData.Socket = socket;
    m_snifferData.Clients = clients;
    m_snifferData.Mutex = mutex;
}


void Sniffer::run()
{
    const char *devName = m_interfaceName.toStdString().c_str();
    const int PACKET_BUFFER_TIMEOUT_MS = 200;
    char errorBuffer[PCAP_ERRBUF_SIZE];
    pcap_t *devHandle;
    struct bpf_program filterProgram;
    int result;

    devHandle = pcap_open_live(devName, BUFSIZ, false, PACKET_BUFFER_TIMEOUT_MS, errorBuffer);

    if (devHandle != nullptr)
    {
        QString filterProgramStr = "udp port ";

        filterProgramStr += m_ts3VoicePort;
        qInfo() << "Successfully opened device" << devName;

        result = pcap_compile(devHandle, &filterProgram, filterProgramStr.toStdString().c_str(), true, PCAP_NETMASK_UNKNOWN);

        if (result == 0)
        {
            qDebug() << "Successfully compiled filter program";

            result = pcap_setfilter(devHandle, &filterProgram);

            if (result == 0)
            {
                qDebug() << "Successfully set filter program";

                result = pcap_loop(devHandle, -1, packetSniffed, reinterpret_cast<u_char *>(&m_snifferData));

                if (result == -1)
                {
                    pcap_perror(devHandle, "pcap_loop failed: ");
                }

                pcap_close(devHandle);
            }

            else
            {
                pcap_perror(devHandle, "Error while setting filter program: ");
            }
        }

        else
        {
            pcap_perror(devHandle, "Error while compiling filter program: ");
        }
    }

    else
    {
        qCritical() << "Failed to open device" << devName;
    }
}

#ifndef QRCODEDECODER_H
#define QRCODEDECODER_H

#include <QObject>
#include <QImage>

class QRCodeDecoder : public QObject
{
    Q_OBJECT

public:
    static QRCodeDecoder& getInstance();
    QString decodeQRCode(const QImage &image);

private:
    explicit QRCodeDecoder(QObject *parent = nullptr);
    ~QRCodeDecoder();
    QImage prepareImage(const QImage &inputImage);

    static const int FIXED_WIDTH = 530;
    static const int FIXED_HEIGHT = 360;

    // Prevent copying
    QRCodeDecoder(const QRCodeDecoder&) = delete;
    QRCodeDecoder& operator=(const QRCodeDecoder&) = delete;
};

#endif // QRCODEDECODER_H
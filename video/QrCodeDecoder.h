#ifndef QRCODEDECODER_H
#define QRCODEDECODER_H

#include <QObject>
#include <QImage>
#include <ZXing/ReadBarcode.h>
#include <ZXing/DecodeHints.h>

class QRCodeDecoder : public QObject
{
    Q_OBJECT

public:
    explicit QRCodeDecoder(QObject *parent = nullptr);
    static QRCodeDecoder& getInstance();
    ~QRCodeDecoder();

    QString decodeQRCode(const QString &imagePath);

private:
    const int FIXED_WIDTH = 530;
    const int FIXED_HEIGHT = 630;

    QImage prepareImage(const QImage &inputImage);
};

#endif // QRCODEDECODER_H
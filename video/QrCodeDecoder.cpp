#include "QRCodeDecoder.h"
#include <QDebug>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ImageView.h>

QRCodeDecoder::QRCodeDecoder(QObject *parent) : QObject(parent)
{
}

QRCodeDecoder::~QRCodeDecoder()
{
}

QRCodeDecoder& QRCodeDecoder::getInstance()
{
    static QRCodeDecoder instance;
    return instance;
}

QString QRCodeDecoder::decodeQRCode(const QString &imagePath)
{
    QImage image(imagePath);
    if (image.isNull()) {
        qDebug() << "Failed to load image from:" << imagePath;
        return QString("Error: Failed to load image");
    }

    QImage preparedImage = prepareImage(image);
    if (preparedImage.isNull()) {
        qDebug() << "Failed to prepare image";
        return QString("Error: Failed to prepare image");
    }

    ZXing::DecodeHints hints;
    hints.setFormats(ZXing::BarcodeFormat::QRCode);

    ZXing::ImageView zxingImage(preparedImage.bits(), preparedImage.width(), preparedImage.height(), ZXing::ImageFormat::RGB);
    auto result = ZXing::ReadBarcode(zxingImage, hints);
    if (result.isValid()) {
        return QString::fromStdString(result.text());
    } else {
        qDebug() << "Failed to decode QR code from:" << imagePath;
        return QString("Error: No QR code found");
    }
}

QImage QRCodeDecoder::prepareImage(const QImage &inputImage)
{
    if (inputImage.isNull()) {
        return QImage();
    }

    if (inputImage.width() == FIXED_WIDTH && inputImage.height() == FIXED_HEIGHT) {
        return inputImage;
    }

    QImage scaledImage = inputImage.scaled(FIXED_WIDTH, FIXED_HEIGHT, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    if (scaledImage.format() != QImage::Format_RGB32) {
        scaledImage = scaledImage.convertToFormat(QImage::Format_RGB32);
    }

    return scaledImage;
}

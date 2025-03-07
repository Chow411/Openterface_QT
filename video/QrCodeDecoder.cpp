#include "QRCodeDecoder.h"
#include <QDebug>

#include <iostream>
#include "ZXingQtReader.h"

using namespace ZXingQt;

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

QString QRCodeDecoder::decodeQRCode(const QImage &image)
{
    if (image.isNull()) {
        qDebug() << "Could not load image:";
        return QString();
    }

    auto options = ReaderOptions()
        .setFormats(BarcodeFormat::MatrixCodes)
        .setTryInvert(false)
        .setTextMode(TextMode::HRI)
        .setMaxNumberOfSymbols(10);

    auto barcodes = ReadBarcodes(image, options);
    for (const auto& barcode : barcodes) {
        qDebug() << "Text:   " << barcode.text();
        qDebug() << "Format: " << barcode.formatName();
        qDebug() << "Content:" << barcode.contentType();
        qDebug() << "";
    }
    return barcodes.isEmpty() ? QString() : barcodes.first().text();
}

#include "QRCodeDecoder.h"
#include <QDebug>
#include <ZXing/ReadBarcode.h>
#include <iostream>
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
        qDebug() << "Invalid or null image provided";
        return QString("Error: Invalid or null image");
    }

    // Check minimum size
    if (image.width() < FIXED_WIDTH || image.height() < FIXED_HEIGHT) {
        qDebug() << "Image too small for" << FIXED_WIDTH << "x" << FIXED_HEIGHT 
                 << "region:" << image.width() << "x" << image.height();
        return QString("Error: Image too small for %1x%2 region").arg(FIXED_WIDTH).arg(FIXED_HEIGHT);
    }

    // Crop the image
    QRect topLeftRegion(0, 0, FIXED_WIDTH, FIXED_HEIGHT);
    QImage croppedImage = image.copy(topLeftRegion);
    if (croppedImage.isNull()) {
        qDebug() << "Failed to crop image to" << FIXED_WIDTH << "x" << FIXED_HEIGHT << "region";
        return QString("Error: Failed to crop image");
    }
    
    #ifdef QT_DEBUG
    croppedImage.save("debug_cropped_image.png", "PNG");
    qDebug() << "Cropped image saved as debug_cropped_image.png";
    #endif

    // Prepare the image
    QImage preparedImage = prepareImage(croppedImage);
    if (preparedImage.isNull()) {
        qDebug() << "Failed to prepare image";
        return QString("Error: Failed to prepare image");
    }

    // Convert to grayscale (more reliable for QR code detection)
    QImage grayImage = preparedImage.convertToFormat(QImage::Format_Grayscale8);
    if (grayImage.isNull()) {
        qDebug() << "Failed to convert image to grayscale";
        return QString("Error: Failed to convert image");
    }

    // Set up ZXing reader options
    // ZXing::ReaderOptions options;
    auto options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::Any);

    // Create ImageView with grayscale data
    ZXing::ImageView zxingImage(
        grayImage.bits(),
        grayImage.width(),
        grayImage.height(),
        ZXing::ImageFormat::Lum  // Grayscale format
    );

    // Read the barcode
    auto result = ZXing::ReadBarcode(zxingImage, options);

    qDebug() << "QR code valid:" << result.isValid();
    if (result.isValid()) {
        QString text = QString::fromStdString(result.text());
        qDebug() << "Decoded QR code text:" << text;
        return text;
    } else {
        qDebug() << "Failed to decode QR code from top-left" << FIXED_WIDTH << "x" 
                 << FIXED_HEIGHT << "region";
        
        return QString("Error: No QR code found in %1x%2 region").arg(FIXED_WIDTH).arg(FIXED_HEIGHT);
    }
}

QImage QRCodeDecoder::prepareImage(const QImage &inputImage)
{
    if (inputImage.isNull()) {
        return QImage();
    }

    QImage image = inputImage;
    // Ensure correct size
    if (image.width() != FIXED_WIDTH || image.height() != FIXED_HEIGHT) {
        image = image.scaled(
            FIXED_WIDTH,
            FIXED_HEIGHT,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation
        );
    }

    return image;
}
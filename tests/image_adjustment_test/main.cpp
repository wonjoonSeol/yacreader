#include "configuration.h"
#include "image_adjustment.h"

#include <QComboBox>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

class ImageAdjustmentTest : public QObject
{
    Q_OBJECT

private slots:
    void offersEachScalingMethodOnce();
    void listsTheScalingMethodsInOrder_data();
    void listsTheScalingMethodsInOrder();
    void offersEveryMethodASettingCanHold();
    void selectsTheItemOfTheStoredMethod_data();
    void selectsTheItemOfTheStoredMethod();
    void storesTheMethodOfTheChosenItem_data();
    void storesTheMethodOfTheChosenItem();
    void readsTheStoredScalingMethod_data();
    void readsTheStoredScalingMethod();
    void readsUnknownStoredValuesAsLanczos4_data();
    void readsUnknownStoredValuesAsLanczos4();
};

namespace {

const QList<ScaleMethod> allMethods { ScaleMethod::Nearest, ScaleMethod::Bilinear, ScaleMethod::Lanczos, ScaleMethod::Lanczos3, ScaleMethod::Lanczos2 };

void addMethodRows()
{
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<int>("position");

    // Lanczos3 and Lanczos2 are stored as 4 and 5, one more than their positions
    QTest::newRow("Nearest") << ScaleMethod::Nearest << 0;
    QTest::newRow("Bilinear") << ScaleMethod::Bilinear << 1;
    QTest::newRow("Lanczos4") << ScaleMethod::Lanczos << 2;
    QTest::newRow("Lanczos3") << ScaleMethod::Lanczos3 << 3;
    QTest::newRow("Lanczos2") << ScaleMethod::Lanczos2 << 4;
}

// Reads the method back from an ini file, as YACReader stores its settings, so the value comes back as a string
ScaleMethod readScalingMethod(const QVariant &stored)
{
    QTemporaryDir dir;
    const QString path = dir.filePath("YACReader.ini");
    if (stored.isValid())
        QSettings(path, QSettings::IniFormat).setValue(SCALING_METHOD, stored);

    QSettings settings(path, QSettings::IniFormat);
    Configuration::getConfiguration().load(&settings);
    return Configuration::getConfiguration().getScalingMethod();
}

}

void ImageAdjustmentTest::offersEachScalingMethodOnce()
{
    const auto &choices = scalingMethodChoices();

    QCOMPARE(choices.size(), allMethods.size());
    for (const auto method : allMethods) {
        const auto offers = [method](const ScalingMethodChoice &choice) { return choice.method == method; };
        QCOMPARE(std::count_if(choices.cbegin(), choices.cend(), offers), 1);
    }
}

void ImageAdjustmentTest::listsTheScalingMethodsInOrder_data()
{
    addMethodRows();
}

void ImageAdjustmentTest::listsTheScalingMethodsInOrder()
{
    QFETCH(ScaleMethod, method);
    QFETCH(int, position);

    QCOMPARE(scalingMethodChoices().at(position).method, method);
}

// Restoring a setting with no item would leave the Options combo empty
void ImageAdjustmentTest::offersEveryMethodASettingCanHold()
{
    QComboBox combo;
    addScalingMethodItems(&combo);

    for (int value = -1; value <= 16; ++value) {
        if (scaleMethodFromInt(value))
            QCOMPARE_GE(combo.findData(value), 0);
    }
}

void ImageAdjustmentTest::selectsTheItemOfTheStoredMethod_data()
{
    addMethodRows();
}

void ImageAdjustmentTest::selectsTheItemOfTheStoredMethod()
{
    QFETCH(ScaleMethod, method);
    QFETCH(int, position);

    QComboBox combo;
    addScalingMethodItems(&combo);
    selectScalingMethod(&combo, method);

    QCOMPARE(combo.currentIndex(), position);
    QCOMPARE(combo.currentText(), scalingMethodChoices().at(position).text());
}

void ImageAdjustmentTest::storesTheMethodOfTheChosenItem_data()
{
    addMethodRows();
}

void ImageAdjustmentTest::storesTheMethodOfTheChosenItem()
{
    QFETCH(ScaleMethod, method);
    QFETCH(int, position);

    QComboBox combo;
    addScalingMethodItems(&combo);
    combo.setCurrentIndex(position);

    QCOMPARE(selectedScalingMethod(&combo), method);
}

void ImageAdjustmentTest::readsTheStoredScalingMethod_data()
{
    addMethodRows();
}

void ImageAdjustmentTest::readsTheStoredScalingMethod()
{
    QFETCH(ScaleMethod, method);

    QCOMPARE(readScalingMethod(static_cast<int>(method)), method);
}

void ImageAdjustmentTest::readsUnknownStoredValuesAsLanczos4_data()
{
    QTest::addColumn<QVariant>("stored");

    QTest::newRow("missing") << QVariant();
    QTest::newRow("-1") << QVariant(-1);
    QTest::newRow("3, left free for Bicubic") << QVariant(3);
    QTest::newRow("6") << QVariant(6);
    QTest::newRow("a name") << QVariant("Lanczos3");
}

void ImageAdjustmentTest::readsUnknownStoredValuesAsLanczos4()
{
    QFETCH(QVariant, stored);

    QCOMPARE(readScalingMethod(stored), ScaleMethod::Lanczos);
}

QTEST_MAIN(ImageAdjustmentTest)

#include "main.moc"

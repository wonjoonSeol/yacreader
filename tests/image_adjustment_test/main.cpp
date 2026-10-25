#include "image_adjustment.h"

#include <QObject>
#include <QTest>

class ImageAdjustmentTest : public QObject
{
    Q_OBJECT

private slots:
    void offersEachScalingMethodOnce();
    void labelsEveryScalingMethod();
    void listsTheScalingMethodsInOrder_data();
    void listsTheScalingMethodsInOrder();
    void findsAStoredMethodByItsValueNotItsPosition();
    void findsNoChoiceForAnUnknownMethod();
    void offersEveryMethodASettingCanHold();
};

void ImageAdjustmentTest::offersEachScalingMethodOnce()
{
    QList<ScaleMethod> methods;
    for (const auto &choice : scalingMethodChoices()) {
        QVERIFY(!methods.contains(choice.method));
        methods << choice.method;
    }

    for (const auto method : { ScaleMethod::Nearest, ScaleMethod::Bilinear, ScaleMethod::Bicubic, ScaleMethod::Lanczos4, ScaleMethod::Lanczos3, ScaleMethod::Lanczos2 })
        QVERIFY(methods.contains(method));
}

void ImageAdjustmentTest::labelsEveryScalingMethod()
{
    for (const auto &choice : scalingMethodChoices())
        QVERIFY(!choice.text().isEmpty());
}

void ImageAdjustmentTest::listsTheScalingMethodsInOrder_data()
{
    QTest::addColumn<int>("position");
    QTest::addColumn<ScaleMethod>("method");
    QTest::addColumn<QString>("text");

    // Bicubic is listed third but stored as 3, and Lanczos4 fourth but stored as 2
    QTest::newRow("Nearest") << 0 << ScaleMethod::Nearest << "Nearest (fast, low quality)";
    QTest::newRow("Bilinear") << 1 << ScaleMethod::Bilinear << "Bilinear";
    QTest::newRow("Bicubic") << 2 << ScaleMethod::Bicubic << "Bicubic";
    QTest::newRow("Lanczos4") << 3 << ScaleMethod::Lanczos4 << "Lanczos4 (more detail, more halos)";
    QTest::newRow("Lanczos3") << 4 << ScaleMethod::Lanczos3 << "Lanczos3 (balanced)";
    QTest::newRow("Lanczos2") << 5 << ScaleMethod::Lanczos2 << "Lanczos2 (fewer halos, less detail)";
}

void ImageAdjustmentTest::listsTheScalingMethodsInOrder()
{
    QFETCH(int, position);
    QFETCH(ScaleMethod, method);
    QFETCH(QString, text);

    const auto choices = scalingMethodChoices();
    QCOMPARE(choices.at(position).method, method);
    QCOMPARE(choices.at(position).text(), text);
    QCOMPARE(scalingMethodChoiceIndex(choices, method), position);
}

// Positions and stored values differ here, as they do once a method is listed out of its value order
void ImageAdjustmentTest::findsAStoredMethodByItsValueNotItsPosition()
{
    const QList<ScalingMethodChoice> choices {
        { ScaleMethod::Lanczos4, "Lanczos4" },
        { ScaleMethod::Nearest, "Nearest" },
        { ScaleMethod::Bilinear, "Bilinear" },
    };

    QCOMPARE(scalingMethodChoiceIndex(choices, ScaleMethod::Lanczos4), 0);
    QCOMPARE(scalingMethodChoiceIndex(choices, ScaleMethod::Nearest), 1);
    QCOMPARE(scalingMethodChoiceIndex(choices, ScaleMethod::Bilinear), 2);
}

void ImageAdjustmentTest::findsNoChoiceForAnUnknownMethod()
{
    // a value stored by a build with more methods
    const auto unknownMethod = static_cast<ScaleMethod>(7);

    QCOMPARE(scalingMethodChoiceIndex(scalingMethodChoices(), unknownMethod), -1);
}

// Restoring a setting with no choice would leave the Options combo unselected
void ImageAdjustmentTest::offersEveryMethodASettingCanHold()
{
    const auto choices = scalingMethodChoices();
    for (int value = -1; value <= 16; ++value)
        QCOMPARE_GE(scalingMethodChoiceIndex(choices, scaleMethodFromSetting(value)), 0);

    // 3 restores Bicubic; builds without it read 3 as Lanczos4
    QCOMPARE(choices.at(scalingMethodChoiceIndex(choices, scaleMethodFromSetting(3))).method, ScaleMethod::Bicubic);
}

QTEST_GUILESS_MAIN(ImageAdjustmentTest)

#include "main.moc"

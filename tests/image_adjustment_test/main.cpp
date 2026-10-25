#include "image_adjustment.h"

#include <QObject>
#include <QTest>

class ImageAdjustmentTest : public QObject
{
    Q_OBJECT

private slots:
    void offersEachScalingMethodOnce();
    void labelsEveryScalingMethod();
    void findsAStoredMethodByItsValueNotItsPosition();
    void findsNoChoiceForAnUnknownMethod();
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

QTEST_GUILESS_MAIN(ImageAdjustmentTest)

#include "main.moc"

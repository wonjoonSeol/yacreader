#include "yacreader_spin_slider_widget.h"

#include <QSignalSpy>
#include <QSpinBox>
#include <QTest>

class SpinSliderWidgetTest : public QObject
{
    Q_OBJECT

private slots:
    void setValueEmitsOnce();
    void userChangeEmitsOnceAfterSetValueCalls();
};

void SpinSliderWidgetTest::setValueEmitsOnce()
{
    YACReaderSpinSliderWidget slider;
    slider.setRange(0, 100);
    QSignalSpy valueChanged(&slider, &YACReaderSpinSliderWidget::valueChanged);

    for (const int value : { 10, 20, 30 }) {
        valueChanged.clear();
        slider.setValue(value);
        QCOMPARE(valueChanged.count(), 1);
        QCOMPARE(valueChanged.at(0).at(0).toInt(), value);
    }
}

// Each setValue() call used to connect the spin box once more, so a later change emitted once per call
void SpinSliderWidgetTest::userChangeEmitsOnceAfterSetValueCalls()
{
    YACReaderSpinSliderWidget slider;
    slider.setRange(0, 100);
    for (const int value : { 10, 20, 30 })
        slider.setValue(value);
    QSignalSpy valueChanged(&slider, &YACReaderSpinSliderWidget::valueChanged);

    auto *spinBox = slider.findChild<QSpinBox *>();
    QVERIFY(spinBox);
    spinBox->stepUp();

    QCOMPARE(valueChanged.count(), 1);
    QCOMPARE(valueChanged.at(0).at(0).toInt(), 31);
}

QTEST_MAIN(SpinSliderWidgetTest)

#include "main.moc"

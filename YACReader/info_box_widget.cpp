#include "info_box_widget.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScreen>

namespace {

// grows with the screen height
int fontSizeFor(const QWidget *parent)
{
    QScreen *screen = parent != nullptr ? parent->window()->screen() : nullptr;
    if (screen == nullptr) {
        screen = QApplication::screens().constFirst();
    }

    const int verticalRes = screen != nullptr ? screen->size().height() : 600;
    if (verticalRes <= 1024) {
        return 12;
    }
    if (verticalRes <= 1200) {
        return 16;
    }
    return 20;
}

}

InfoBoxWidget::InfoBoxWidget(QWidget *parent)
    : QWidget(parent), fontSizePx(fontSizeFor(parent))
{
    auto layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);

    textLabel = new QLabel(this);
    textLabel->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    textLabel->setWordWrap(false); // Wrapping needs to be disabled, we need to show all the text in one line

    setContentsMargins(fontSizePx * 2.3, fontSizePx / 2.3, fontSizePx * 2.3, fontSizePx / 2.3);

    // Instead of fixed size, allow dynamic sizing
    textLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    layout->addWidget(textLabel, 0, Qt::AlignCenter);
    setLayout(layout);

    initTheme(this);
}

void InfoBoxWidget::setText(const QString &text)
{
    textLabel->setText(text);
}

void InfoBoxWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    painter.fillRect(rect(), backgroundColor);
}

void InfoBoxWidget::applyTheme(const Theme &theme)
{
    const auto viewerTheme = theme.viewer;

    backgroundColor = viewerTheme.infoBackgroundColor;

    textLabel->setStyleSheet(viewerTheme.infoLabelQSS.arg(fontSizePx));

    update();
}

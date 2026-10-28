#include "configuration.h"

#include <QCoreApplication>
#include <QFile>
#include <QMessageBox>
#include <QStringList>
#include <QTextStream>

Configuration::Configuration()
{
}

QSettings *Configuration::getSettings()
{
    return settings;
}

void Configuration::load(QSettings *settings)
{
    this->settings = settings;

    // TODO set defaults
    if (!settings->contains(PATH))
        settings->setValue(PATH, ".");
    if (!settings->contains(GO_TO_FLOW_SIZE))
        settings->setValue(GO_TO_FLOW_SIZE, QSize(126, 200));
    if (!settings->contains(MAG_GLASS_SIZE))
        settings->setValue(MAG_GLASS_SIZE, QSize(350, 175));
    if (!settings->contains(MAG_GLASS_ZOOM))
        settings->setValue(MAG_GLASS_ZOOM, 0.5);
    if (!settings->contains(MAG_GLASS_CIRCULAR))
        settings->setValue(MAG_GLASS_CIRCULAR, false);
    if (!settings->contains(MAG_GLASS_RING))
        settings->setValue(MAG_GLASS_RING, true);
    if (!settings->contains(MAG_GLASS_EDGE_EASE))
        settings->setValue(MAG_GLASS_EDGE_EASE, true);
    if (!settings->contains(FLOW_TYPE))
        settings->setValue(FLOW_TYPE, 0);
    if (!settings->contains(FULLSCREEN))
        settings->setValue(FULLSCREEN, false);
    if (!settings->contains(DOUBLE_PAGE))
        settings->setValue(DOUBLE_PAGE, false);
    if (!settings->contains(SHOW_TOOLBARS))
        settings->setValue(SHOW_TOOLBARS, true);
    if (!settings->contains(QUICK_NAVI_MODE))
        settings->setValue(QUICK_NAVI_MODE, false);
}
void Configuration::updateOpenRecentList(QString path)
{
    QStringList list = openRecentList();
    list.removeAll(path);
    list.prepend(path);
    // TODO: Make list lenght configurable
    while (list.length() > getOpenRecentSize()) {
        list.removeLast();
    }
    settings->setValue("recentFiles", list);
}

void Configuration::setContinuousScrollFitToHeight(bool b)
{
    if (b == getContinuousScrollFitToHeight())
        return;
    settings->setValue(CONTINUOUS_SCROLL_FIT_TO_HEIGHT, b);
    emit imageOptionsChanged();
}

void Configuration::setEnlargeImages(bool b)
{
    if (b == getEnlargeImages())
        return;
    settings->setValue(ENLARGE_IMAGES, b);
    emit imageOptionsChanged();
}

void Configuration::setScalingMethod(ScaleMethod method)
{
    if (method == getScalingMethod())
        return;
    settings->setValue(SCALING_METHOD, static_cast<int>(method));
    emit imageOptionsChanged();
}

bool Configuration::storeFilterLevel(const ImageFilterSpec &filter, int level)
{
    if (level == getFilterLevel(filter))
        return false;
    settings->setValue(filter.settingsKey, level);
    return true;
}

void Configuration::setFilterLevel(const ImageFilterSpec &filter, int level)
{
    if (storeFilterLevel(filter, level))
        emit filtersChanged(getFilterLevel(brightnessSpec), getFilterLevel(contrastSpec), getFilterLevel(gammaSpec));
}

void Configuration::setFilterLevels(int brightness, int contrast, int gamma)
{
    bool changed = storeFilterLevel(brightnessSpec, brightness);
    changed = storeFilterLevel(contrastSpec, contrast) || changed;
    changed = storeFilterLevel(gammaSpec, gamma) || changed;
    if (changed)
        emit filtersChanged(brightness, contrast, gamma);
}

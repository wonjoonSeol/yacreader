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

bool Configuration::storeIfChanged(const char *key, const QVariant &current, const QVariant &value)
{
    if (value == current)
        return false;
    settings->setValue(key, value);
    return true;
}

void Configuration::setContinuousScrollFitToHeight(bool b)
{
    if (storeIfChanged(CONTINUOUS_SCROLL_FIT_TO_HEIGHT, getContinuousScrollFitToHeight(), b))
        emit imageOptionsChanged();
}

void Configuration::setEnlargeImages(bool b)
{
    if (storeIfChanged(ENLARGE_IMAGES, getEnlargeImages(), b))
        emit imageOptionsChanged();
}

void Configuration::setScalingMethod(ScaleMethod method)
{
    if (storeIfChanged(SCALING_METHOD, static_cast<int>(getScalingMethod()), static_cast<int>(method)))
        emit imageOptionsChanged();
}

void Configuration::setFilterLevel(const ImageFilterSpec &filter, int level)
{
    if (storeIfChanged(filter.settingsKey, getFilterLevel(filter), level))
        emit filtersChanged(getFilterLevel(brightnessSpec), getFilterLevel(contrastSpec), getFilterLevel(gammaSpec));
}

void Configuration::setFilterLevels(int brightness, int contrast, int gamma)
{
    const auto store = [this](const ImageFilterSpec &filter, int level) {
        return storeIfChanged(filter.settingsKey, getFilterLevel(filter), level);
    };
    bool changed = store(brightnessSpec, brightness);
    changed = store(contrastSpec, contrast) || changed;
    changed = store(gammaSpec, gamma) || changed;
    if (changed)
        emit filtersChanged(brightness, contrast, gamma);
}

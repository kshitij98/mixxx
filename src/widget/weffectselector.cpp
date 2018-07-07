#include <QtDebug>

#include "widget/weffectselector.h"

#include "effects/effectsmanager.h"
#include "widget/effectwidgetutils.h"

WEffectSelector::WEffectSelector(QWidget* pParent, EffectsManager* pEffectsManager)
        : QComboBox(pParent),
          WBaseWidget(this),
          m_pEffectsManager(pEffectsManager),
          m_scaleFactor(1.0) {
    // Prevent this widget from getting focused to avoid
    // interfering with using the library via keyboard.
    setFocusPolicy(Qt::NoFocus);
}

void WEffectSelector::setup(const QDomNode& node, const SkinContext& context) {
    m_scaleFactor = context.getScaleFactor();

    // EffectWidgetUtils propagates NULLs so this is all safe.
    m_pChainSlot = EffectWidgetUtils::getEffectChainSlotFromNode(
            node, context, m_pEffectsManager);
    m_pEffectSlot = EffectWidgetUtils::getEffectSlotFromNode(
            node, context, m_pChainSlot);

    if (m_pEffectSlot != nullptr) {
        connect(m_pEffectsManager, SIGNAL(visibleEffectsUpdated()),
                this, SLOT(populate()));
        connect(m_pEffectSlot.data(), SIGNAL(updated()),
                this, SLOT(slotEffectUpdated()));
        connect(this, SIGNAL(currentIndexChanged(int)),
                this, SLOT(slotEffectSelected(int)));
    } else {
        SKIN_WARNING(node, context)
                << "EffectSelector node could not attach to effect slot.";
    }

    populate();
}


void WEffectSelector::populate() {
    blockSignals(true);
    clear();

    const QList<EffectManifestPointer> visibleEffectManifests =
            m_pEffectsManager->getVisibleEffectManifests();
    QFontMetrics metrics(font());

    for (int i = 0; i < visibleEffectManifests.size(); ++i) {
        const EffectManifestPointer pManifest = visibleEffectManifests.at(i);
        QString elidedDisplayName = metrics.elidedText(pManifest->displayName(),
                                                       Qt::ElideMiddle,
                                                       width() - 2);
        addItem(elidedDisplayName, QVariant(pManifest->id()));

        // NOTE(Be): Using \n instead of : as the separator does not work in
        // QComboBox item tooltips.
        // TODO(Be): Check if this is also the case with Qt5.
        //: %1 = effect name; %2 = effect description
        QString description = tr("%1: %2").arg(pManifest->name(),
                                               pManifest->description());
        // The <span/> is a hack to get Qt to treat the string as rich text so
        // it automatically wraps long lines.
        setItemData(i, QVariant("<span/>" + description), Qt::ToolTipRole);
    }

    //: Displayed when no effect is loaded
    addItem(tr("None"), QVariant());
    setItemData(visibleEffectManifests.size(), QVariant(tr("No effect loaded.")),
                Qt::ToolTipRole);

    slotEffectUpdated();
    blockSignals(false);
}

void WEffectSelector::slotEffectSelected(int newIndex) {
    const QString id = itemData(newIndex).toString();
    m_pChainSlot->loadEffect(m_pEffectSlot->getEffectSlotNumber(0), id);
    setBaseTooltip(itemData(newIndex, Qt::ToolTipRole).toString());
}

void WEffectSelector::slotEffectUpdated() {
    int newIndex;

    if (m_pEffectSlot != nullptr) {
        if (pEffectSlot->getManifest() != nullptr) {
            EffectManifestPointer pManifest = pEffectSlot->getManifest();
            newIndex = findData(QVariant(pManifest->id()));
        } else {
            newIndex = findData(QVariant());
        }
    } else {
        newIndex = findData(QVariant());
    }

    if (newIndex != -1 && newIndex != currentIndex()) {
        setCurrentIndex(newIndex);
        setBaseTooltip(itemData(newIndex, Qt::ToolTipRole).toString());
    }
}

bool WEffectSelector::event(QEvent* pEvent) {
    if (pEvent->type() == QEvent::ToolTip) {
        updateTooltip();
    } else if (pEvent->type() == QEvent::FontChange) {
        const QFont& fonti = font();
        // Change the new font on the fly by casting away its constancy
        // using setFont() here, would results into a recursive loop
        // resetting the font to the original css values.
        // Only scale pixel size fonts, point size fonts are scaled by the OS
        if (fonti.pixelSize() > 0) {
            const_cast<QFont&>(fonti).setPixelSize(fonti.pixelSize() * m_scaleFactor);
        }
        // repopulate to add text according to the new font measures
        populate();
    }

    return QComboBox::event(pEvent);
}

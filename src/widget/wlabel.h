/***************************************************************************
                          wlabel.h  -  description
                             -------------------
    begin                : Wed Jan 5 2005
    copyright            : (C) 2003 by Tue Haste Andersen
    email                : haste@diku.dk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef WLABEL_H
#define WLABEL_H

#include <QLabel>
#include <QEvent>

#include "widget/wbasewidget.h"
#include "skin/skincontext.h"

class WLabel : public QLabel, public WBaseWidget {
    Q_OBJECT
  public:
    explicit WLabel(QWidget* pParent=nullptr);

    virtual void setup(const QDomNode& node, const SkinContext& context);

    QString text() const;
    void setText(const QString& text);

  protected:
    bool event(QEvent* pEvent) override;
    void resizeEvent(QResizeEvent* event) override;
    void fillDebugTooltip(QStringList* debug) override;
    QString m_skinText;
    // Foreground and background colors.
    QColor m_qFgColor;
    QColor m_qBgColor;
  private:
    QString m_longText;
    Qt::TextElideMode m_elideMode;
    double m_scaleFactor;
};

#endif

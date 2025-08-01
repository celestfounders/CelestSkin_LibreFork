/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <QtInstanceBox.hxx>
#include <QtInstanceBox.moc>

QtInstanceBox::QtInstanceBox(QWidget* pWidget)
    : QtInstanceContainer(pWidget)
{
    assert(qobject_cast<QBoxLayout*>(pWidget->layout()) && "widget doesn't have a box layout");
}

void QtInstanceBox::reorder_child(weld::Widget* pWidget, int nPosition)
{
    SolarMutexGuard g;

    GetQtInstance().RunInMainThread([&] {
        QtInstanceWidget* pQtInstanceWidget = dynamic_cast<QtInstanceWidget*>(pWidget);
        assert(pQtInstanceWidget);
        QWidget* pQWidget = pQtInstanceWidget->getQWidget();
        assert(pQWidget);

        getLayout().removeWidget(pQWidget);
        getLayout().insertWidget(nPosition, pQWidget);
    });
}

void QtInstanceBox::sort_native_button_order() { assert(false && "Not implemented yet"); }

QBoxLayout& QtInstanceBox::getLayout() const
{
    return static_cast<QBoxLayout&>(QtInstanceContainer::getLayout());
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */

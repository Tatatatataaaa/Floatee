/********************************************************************************
** Form generated from reading UI file 'teeyes.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TEEYES_H
#define UI_TEEYES_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TeEyes
{
public:
    QLabel *label;

    void setupUi(QWidget *TeEyes)
    {
        if (TeEyes->objectName().isEmpty())
            TeEyes->setObjectName("TeEyes");
        TeEyes->resize(510, 346);
        label = new QLabel(TeEyes);
        label->setObjectName("label");
        label->setGeometry(QRect(160, 160, 40, 12));
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Ignored);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy);

        retranslateUi(TeEyes);

        QMetaObject::connectSlotsByName(TeEyes);
    } // setupUi

    void retranslateUi(QWidget *TeEyes)
    {
        TeEyes->setWindowTitle(QCoreApplication::translate("TeEyes", "TeEyes", nullptr));
        label->setText(QCoreApplication::translate("TeEyes", "Background", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TeEyes: public Ui_TeEyes {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TEEYES_H

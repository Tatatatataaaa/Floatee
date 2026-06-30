/********************************************************************************
** Form generated from reading UI file 'floatee.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FLOATEE_H
#define UI_FLOATEE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Floatee
{
public:
    QWidget *centralwidget;

    void setupUi(QMainWindow *Floatee)
    {
        if (Floatee->objectName().isEmpty())
            Floatee->setObjectName("Floatee");
        Floatee->resize(313, 283);
        centralwidget = new QWidget(Floatee);
        centralwidget->setObjectName("centralwidget");
        Floatee->setCentralWidget(centralwidget);

        retranslateUi(Floatee);

        QMetaObject::connectSlotsByName(Floatee);
    } // setupUi

    void retranslateUi(QMainWindow *Floatee)
    {
        Floatee->setWindowTitle(QCoreApplication::translate("Floatee", "Floatee", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Floatee: public Ui_Floatee {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FLOATEE_H

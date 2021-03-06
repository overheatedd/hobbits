#include "bitserror.h"
#include "cmath"
#include "ctime"
#include "ui_bitserror.h"
#include <QBitArray>
#include <QDebug>
#include <random>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

BitsError::BitsError() :
    ui(new Ui::BitsError()),
    m_stateHelper(new PluginStateHelper())
{
    m_stateHelper->addParameter("error_coeff", QJsonValue::Double, [this](QJsonValue value) {
        ui->coeffInput->setText(QString("%1").arg(value.toDouble()));
        return true;
    }, [this]() {
        return QJsonValue(ui->coeffInput->text().toDouble());
    });

    m_stateHelper->addSpinBoxIntParameter("error_exp", [this](){return ui->expInput;});

    m_stateHelper->addParameter("error_type", QJsonValue::String, [this](QJsonValue value) {
        if (value.toString() == "periodic") {
            ui->periodicOpt->setChecked(true);
        }
        else {
            ui->gaussianOpt->setChecked(true);
        }
        return true;
    }, [this]() {
        if (ui->periodicOpt->isChecked()) {
            return QJsonValue("periodic");
        }
        else {
            return QJsonValue("gaussian");
        }
    });
}

QString BitsError::getName()
{
    return "Bit Error";
}

void BitsError::applyToWidget(QWidget *widget)
{
    ui->setupUi(widget);
}

void BitsError::provideCallback(QSharedPointer<PluginCallback> pluginCallback)
{
    // the plugin callback allows the self-triggering of operateOnContainers
    m_pluginCallback = pluginCallback;
}

QSharedPointer<const OperatorResult> BitsError::getGaussianErrorBits(QSharedPointer<const BitContainer> input,
                                                  const QJsonObject &recallablePluginState,
                                                  QSharedPointer<ActionProgress> progressTracker) {
    qint64 bitLength = input->bits()->sizeInBits();

    QSharedPointer<BitArray> outputBits = QSharedPointer<BitArray>(new BitArray(input->bits().data()));

    double coeff = (recallablePluginState.value("error_coeff")).toDouble();
    int exp = (recallablePluginState.value("error_exp")).toInt();

    double TrueBer = ((coeff * (pow(10, exp))));

    qint64 numBitsToFlip = qint64(floor(bitLength * TrueBer));
    qint64 incr = bitLength / numBitsToFlip;
    qint64 start = 0;
    qint64 end = bitLength / 2;
    qint64 counter = 0;
    qint64 mean = qint64(floor(end / 2));
    qint64 stddev = qint64(floor(end / 6)); // Empirical Rule

    std::normal_distribution<double> distribution(0, stddev);
    std::default_random_engine generator;

    while (counter != numBitsToFlip) {
        qint64 number = qint64(distribution(generator)) + mean;

        if (number < 0) {
            number += bitLength;
            if ((outputBits->at(number - 1) ^ 1) == 1) {
                outputBits->set(number - 1, true);
            }
            else {
                outputBits->set(number - 1, false);
            }
        }
        else if (number > bitLength) {
            number %= bitLength;
            if ((outputBits->at(number - 1) ^ 1) == 1) {
                outputBits->set(number - 1, true);
            }
            else {
                outputBits->set(number - 1, false);
            }
        }
        else if (number >= start && number <= end) {
            if ((outputBits->at(number - 1) ^ 1) == 1) {
                outputBits->set(number - 1, true);
            }
            else {
                outputBits->set(number - 1, false);
            }
        }

        mean += incr;
        start += incr;
        end += incr;
        counter++;

        progressTracker->setProgress(counter, numBitsToFlip);
        if (progressTracker->getCancelled()) {
            return OperatorResult::error("Process cancelled");
        }
    }

    QSharedPointer<BitContainer> bitContainer = QSharedPointer<BitContainer>(new BitContainer());
    bitContainer->setBits(outputBits);
    bitContainer->setName(QString("%1e%2 BER <- %3").arg(coeff).arg(exp).arg(input->name()));

    return OperatorResult::result({bitContainer}, recallablePluginState);
}

QSharedPointer<const OperatorResult> BitsError::getPeriodicErrorBits(QSharedPointer<const BitContainer> input,
                                                    const QJsonObject &recallablePluginState,
                                                    QSharedPointer<ActionProgress> progressTracker) {

    qint64 bitLength = input->bits()->sizeInBits();

    QSharedPointer<BitArray> outputBits = QSharedPointer<BitArray>(new BitArray(input->bits().data()));

    double coeff = recallablePluginState.value("error_coeff").toDouble();
    int exp = int(recallablePluginState.value("error_exp").toDouble());

    double TrueBer = ((coeff * (pow(10, exp))));
    double ber = 1 / TrueBer;


    int skipStep = int(floor(ber));

    if (bitLength > 0) {
        for (qint64 i = skipStep - 1; i < bitLength; i += skipStep) {
            if ((outputBits->at(i) ^ 1) == 0) {
                outputBits->set(i, false);
            }
            else {
                outputBits->set(i, true);
            }

            progressTracker->setProgress(i, bitLength);
            if (progressTracker->getCancelled()) {
                return OperatorResult::error("Process cancelled");
            }
        }
    }

    QSharedPointer<BitContainer> bitContainer = QSharedPointer<BitContainer>(new BitContainer());
    bitContainer->setBits(outputBits);
    bitContainer->setName(QString("%1e%2 BER <- %3").arg(coeff).arg(exp).arg(input->name()));

    return OperatorResult::result({ bitContainer }, recallablePluginState);
}

QJsonObject BitsError::getStateFromUi()
{
    return m_stateHelper->getPluginStateFromUi();
}

bool BitsError::setPluginStateInUi(const QJsonObject &pluginState)
{
    return m_stateHelper->applyPluginStateToUi(pluginState);
}

bool BitsError::canRecallPluginState(const QJsonObject &pluginState)
{
    return m_stateHelper->validatePluginState(pluginState);
}

int BitsError::getMinInputContainers(const QJsonObject &pluginState)
{
    Q_UNUSED(pluginState)
    return 1;
}

int BitsError::getMaxInputContainers(const QJsonObject &pluginState)
{
    Q_UNUSED(pluginState)
    return 1;
}

QSharedPointer<const OperatorResult> BitsError::operateOnContainers(
        QList<QSharedPointer<const BitContainer>> inputContainers,
        const QJsonObject &recallablePluginState,
        QSharedPointer<ActionProgress> progressTracker)
{
    if (inputContainers.size() != 1) {
        return OperatorResult::error("Requires a single bit container as input");
    }

    if (recallablePluginState.value("error_type").toString() == "gaussian") {
        return getGaussianErrorBits(inputContainers.at(0), recallablePluginState, progressTracker);
    }
    else {
        return getPeriodicErrorBits(inputContainers.at(0), recallablePluginState, progressTracker);
    }
}

OperatorInterface* BitsError::createDefaultOperator()
{
    return new BitsError();
}

void BitsError::previewBits(QSharedPointer<BitContainerPreview> container)
{
    Q_UNUSED(container)
}

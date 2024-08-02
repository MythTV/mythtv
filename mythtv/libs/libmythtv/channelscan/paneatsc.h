/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef PANE_ATSC_H
#define PANE_ATSC_H

#include <algorithm>

// MythTV headers
#include "channelscanmiscsettings.h"
#include "frequencytablesetting.h"
#include "modulationsetting.h"
#include "frequencytables.h"

class PaneATSC : public GroupSetting
{
    Q_OBJECT

  public:
    PaneATSC(const QString &target, StandardSetting *setting) :
        m_atscTable(new ScanFrequencyTable()),
        m_atscModulation(new ScanATSCModulation()),
        m_transportStart(new TransMythUIComboBoxSetting()),
        m_transportEnd(new TransMythUIComboBoxSetting()),
        m_transportCount(new GroupSetting())
    {
        setVisible(false);

        connect(m_atscTable, qOverload<const QString&>(&StandardSetting::valueChanged),
                this,        &PaneATSC::FreqTableChanged);

        connect(m_atscModulation, qOverload<const QString&>(&StandardSetting::valueChanged),
                this,             &PaneATSC::ModulationChanged);

        m_transportStart->setLabel(tr("First Channel"));
        m_transportStart->setHelpText(tr("Start scanning at this channel."));

        m_transportEnd->setLabel(tr("Last Channel"));
        m_transportEnd->setHelpText(tr("Stop scanning after this channel."));

        m_transportCount->setLabel(tr("Channel Count"));
        m_transportCount->setHelpText(tr("Total number of channels to scan."));
        m_transportCount->setReadOnly(true);

        auto *range = new GroupSetting();
        range->setLabel(tr("Scanning Range"));
        range->addChild(m_transportStart);
        range->addChild(m_transportEnd);
        range->addChild(m_transportCount);

        setting->addTargetedChildren(target,
                                     {this, m_atscTable, m_atscModulation, range});

        connect(m_transportStart, qOverload<const QString&>(&StandardSetting::valueChanged),
                this,             &PaneATSC::TransportRangeChanged);
        connect(m_transportEnd,   qOverload<const QString&>(&StandardSetting::valueChanged),
                this,             &PaneATSC::TransportRangeChanged);

        ResetTransportRange();
    }

    ~PaneATSC() override
    {
        while (!m_tables.empty())
        {
            delete m_tables.back();
            m_tables.pop_back();
        }
    }

    void SetFrequencyTable(const QString &atsc_table)
        { m_atscTable->setValue(atsc_table); }

    QString GetFrequencyTable(void) const
        { return m_atscTable->getValue(); }
    QString GetModulation(void) const
        { return m_atscModulation->getValue(); }
    bool GetFrequencyTableRange(
        QString &start, QString &end) const
    {
        if (!m_transportStart->size() || !m_transportEnd->size())
            return false;

        start = m_transportStart->getValue();
        end   = m_transportEnd->getValue();

        return !start.isEmpty() && !end.isEmpty();
    }

  protected slots:
    void FreqTableChanged(const QString &freqtbl)
    {
        if (freqtbl == "us")
            m_atscModulation->setValue(0);
        else if (m_atscModulation->getValue() == "vsb8")
            m_atscModulation->setValue(1);

        ResetTransportRange();
    }

    void ModulationChanged(const QString &/*modulation*/)
    {
        ResetTransportRange();
    }

    void TransportRangeChanged(const QString &/*range*/)
    {
        int a = m_transportStart->getValueIndex(m_transportStart->getValue());
        int b = m_transportEnd->getValueIndex(m_transportEnd->getValue());
        if (b < a)
        {
            m_transportEnd->setValue(m_transportStart->getValue());
            b = a;
        }

        int diff = std::max(b + 1 - a, 0);
        m_transportCount->setValue(QString::number(diff));
    }

  protected:
    void ResetTransportRange(void)
    {
        m_transportStart->clearSelections();
        m_transportEnd->clearSelections();
        m_transportCount->setValue(QString::number(0));

        FetchFrequencyTables();

        bool first = true;
        for (auto it = m_tables.begin(); it != m_tables.end(); ++it)
        {
            auto next = it;
            ++next;

            const FrequencyTable &ft = **it;
            int     name_num         = ft.m_nameOffset;
            QString strNameFormat    = ft.m_nameFormat;
            uint    freq             = ft.m_frequencyStart;
            while (freq <= ft.m_frequencyEnd)
            {
                QString name = strNameFormat;
                if (strNameFormat.indexOf("%") >= 0)
                    name = strNameFormat.arg(name_num);

                m_transportStart->addSelection(name, name, first);
                first = false;

                bool last = (next == m_tables.end()) &&
                    ((freq + ft.m_frequencyStep) >= ft.m_frequencyEnd);
                m_transportEnd->addSelection(name, name, last);

                name_num++;
                freq += ft.m_frequencyStep;
            }
        }
    }

    void FetchFrequencyTables(void)
    {
        QString format     = "atsc";
        QString modulation = GetModulation();
        QString country    = GetFrequencyTable();

        const QString new_tables_sig =
            QString("%1_%2_%3").arg(format, modulation, country);

        if (new_tables_sig != m_tablesSig)
        {
            while (!m_tables.empty())
            {
                delete m_tables.back();
                m_tables.pop_back();
            }

            m_tablesSig = new_tables_sig;

            m_tables = get_matching_freq_tables(
                format, modulation, country);
        }
    }

  protected:
    ScanFrequencyTable         *m_atscTable       {nullptr};
    ScanATSCModulation         *m_atscModulation  {nullptr};
    TransMythUIComboBoxSetting *m_transportStart  {nullptr};
    TransMythUIComboBoxSetting *m_transportEnd    {nullptr};
    GroupSetting               *m_transportCount  {nullptr};
    QString                     m_tablesSig;
    freq_table_list_t           m_tables;
};

#endif // PANE_ATSC_H

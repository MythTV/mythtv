/* -*- Mode: c++ -*-
 * vim: set expandtab tabstop=4 shiftwidth=4:
 */

#ifndef PANE_ATSC_H
#define PANE_ATSC_H

#include <algorithm>
using namespace std;

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
        m_atsc_table(new ScanFrequencyTable()),
        m_atsc_modulation(new ScanATSCModulation())
    {
        setVisible(false);

        connect(m_atsc_table, SIGNAL(valueChanged(    const QString&)),
                this,       SLOT(  FreqTableChanged(const QString&)));

        connect(m_atsc_modulation, SIGNAL(valueChanged(     const QString&)),
                this,            SLOT(  ModulationChanged(const QString&)));

        auto *range = new GroupSetting();
        m_transport_start = new TransMythUIComboBoxSetting();
        m_transport_end   = new TransMythUIComboBoxSetting();
        m_transport_count = new TransTextEditSetting();
        range->setLabel(tr("Scanning Range"));
        range->addChild(m_transport_start);
        range->addChild(m_transport_end);
        range->addChild(m_transport_count);

        setting->addTargetedChildren(target,
                                     {this, m_atsc_table, m_atsc_modulation, range});

        connect(m_transport_start, SIGNAL(valueChanged(       const QString&)),
                this,            SLOT(  TransportRangeChanged(const QString&)));
        connect(m_transport_end,   SIGNAL(valueChanged(       const QString&)),
                this,            SLOT(  TransportRangeChanged(const QString&)));

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
        { m_atsc_table->setValue(atsc_table); }

    QString GetFrequencyTable(void) const
        { return m_atsc_table->getValue(); }
    QString GetModulation(void) const
        { return m_atsc_modulation->getValue(); }
    bool GetFrequencyTableRange(
        QString &start, QString &end) const
    {
        if (!m_transport_start->size() || !m_transport_end->size())
            return false;

        start = m_transport_start->getValue();
        end   = m_transport_end->getValue();

        return !start.isEmpty() && !end.isEmpty();
    }

  protected slots:
    void FreqTableChanged(const QString &freqtbl)
    {
        if (freqtbl == "us")
            m_atsc_modulation->setValue(0);
        else if (m_atsc_modulation->getValue() == "vsb8")
            m_atsc_modulation->setValue(1);

        ResetTransportRange();
    }

    void ModulationChanged(const QString &/*modulation*/)
    {
        ResetTransportRange();
    }

    void TransportRangeChanged(const QString &/*range*/)
    {
        int a = m_transport_start->getValueIndex(m_transport_start->getValue());
        int b = m_transport_end->getValueIndex(m_transport_end->getValue());
        if (b < a)
        {
            m_transport_end->setValue(m_transport_start->getValue());
            b = a;
        }

        int diff = max(b + 1 - a, 0);
        m_transport_count->setValue(QString::number(diff));
    }

  protected:
    void ResetTransportRange(void)
    {
        m_transport_start->clearSelections();
        m_transport_end->clearSelections();
        m_transport_count->setValue(QString::number(0));

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

                m_transport_start->addSelection(name, name, first);
                first = false;

                bool last = (next == m_tables.end()) &&
                    ((freq + ft.m_frequencyStep) >= ft.m_frequencyEnd);
                m_transport_end->addSelection(name, name, last);

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
            QString("%1_%2_%3").arg(format).arg(modulation).arg(country);

        if (new_tables_sig != m_tables_sig)
        {
            while (!m_tables.empty())
            {
                delete m_tables.back();
                m_tables.pop_back();
            }

            m_tables_sig = new_tables_sig;

            m_tables = get_matching_freq_tables(
                format, modulation, country);
        }
    }

  protected:
    ScanFrequencyTable         *m_atsc_table      {nullptr};
    ScanATSCModulation         *m_atsc_modulation {nullptr};
    TransMythUIComboBoxSetting *m_transport_start {nullptr};
    TransMythUIComboBoxSetting *m_transport_end   {nullptr};
    TransTextEditSetting       *m_transport_count {nullptr};
//  QString                     m_old_freq_table;
    QString                     m_tables_sig;
    freq_table_list_t           m_tables;
};

#endif // PANE_ATSC_H

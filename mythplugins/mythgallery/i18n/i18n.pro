include ( ../../settings.pro )

TEMPLATE = app
CONFIG -= moc qt

trans.path = $${PREFIX}/share/mythtv/i18n/
trans.files  = mythgallery_it.qm mythgallery_es.qm mythgallery_ca.qm
trans.files += mythgallery_nl.qm mythgallery_de.qm mythgallery_dk.qm
trans.files += mythgallery_pt.qm mythgallery_sv.qm mythgallery_fr.qm
trans.files += mythgallery_ja.qm mythgallery_si.qm mythgallery_nb.qm

INSTALLS += trans

SOURCES += dummy.c

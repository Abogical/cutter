#include <QShortcut>
#include <QModelIndex>

#include "MainWindow.h"
#include "utils/Helpers.h"

#include "VTablesWidget.h"
#include "ui_VTablesWidget.h"

VTableModel::VTableModel(QList<VTableDescription> *vtables, QObject *parent)
    : QAbstractItemModel(parent),
      vtables(vtables)
{
}

QModelIndex VTableModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column, (quintptr) parent.isValid() ? parent.row() : -1);
}

QModelIndex VTableModel::parent(const QModelIndex &index) const
{
    return index.isValid() && index.internalId() != (quintptr) - 1 ?
           this->index(index.internalId(), index.column()) : QModelIndex();
}

int VTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? (parent.parent().isValid() ? 0 : vtables->at(
                                   parent.row()).methods.count()) : vtables->count();
}

int VTableModel::columnCount(const QModelIndex &) const
{
    return Columns::COUNT;
}

QVariant VTableModel::data(const QModelIndex &index, int role) const
{
    QModelIndex parent = index.parent();
    if (parent.isValid()) {
        const ClassMethodDescription &res = vtables->at(parent.row()).methods.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case NAME:
                return res.name;
            case ADDRESS:
                return RAddressString(res.addr);
            default:
                break;
            }
        case VTableDescriptionRole:
            return QVariant::fromValue(res);
        default:
            break;
        }
    } else
        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case NAME:
                return tr("VTable ") + QString::number(index.row() + 1);
            case ADDRESS:
                return RAddressString(vtables->at(index.row()).addr);
            default:
                break;
            }
        case VTableDescriptionRole: {
            const VTableDescription &res = vtables->at(index.row());
            return QVariant::fromValue(res);
        }
        default:
            break;
        }
    return QVariant();
}

QVariant VTableModel::headerData(int section, Qt::Orientation, int role) const
{
    switch (role) {
    case Qt::DisplayRole:
        switch (section) {
        case NAME:
            return tr("Name");
        case ADDRESS:
            return tr("Address");
        default:
            break;
        }
    default:
        break;
    }
    return QVariant();
}

void VTableModel::beginReload()
{
    beginResetModel();
}

void VTableModel::endReload()
{
    endResetModel();
}

VTableSortFilterProxyModel::VTableSortFilterProxyModel(VTableModel *model, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSourceModel(model);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setFilterKeyColumn(VTableModel::NAME);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    setRecursiveFilteringEnabled(true);
#endif
}

bool VTableSortFilterProxyModel::filterAcceptsRow(int source_row,
                                                  const QModelIndex &source_parent) const
{
    if (QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent))
        return true;
    if (source_parent.isValid())
        return QSortFilterProxyModel::filterAcceptsRow(source_parent.row(), QModelIndex());
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    else {
        QAbstractItemModel *const model = sourceModel();
        const QModelIndex source  = model->index(source_row, 0, QModelIndex());
        const int rows = model->rowCount(source);
        for (int i = 0; i < rows; ++i)
            if (QSortFilterProxyModel::filterAcceptsRow(i, source))
                return true;
    }
#endif
    return false;
}


VTablesWidget::VTablesWidget(MainWindow *main, QAction *action) :
    CutterDockWidget(main, action),
    ui(new Ui::VTablesWidget)
{
    ui->setupUi(this);

    model = new VTableModel(&vtables, this);
    proxy = new VTableSortFilterProxyModel(model);

    ui->vTableTreeView->setModel(proxy);
    ui->vTableTreeView->sortByColumn(VTableModel::ADDRESS, Qt::AscendingOrder);

    // Esc to clear the filter entry
    QShortcut *clear_shortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(clear_shortcut, &QShortcut::activated, ui->quickFilterView, &QuickFilterView::clearFilter);

    // Ctrl-F to show/hide the filter entry
    QShortcut *search_shortcut = new QShortcut(QKeySequence::Find, this);
    connect(search_shortcut, &QShortcut::activated, ui->quickFilterView, &QuickFilterView::showFilter);
    search_shortcut->setContext(Qt::WidgetWithChildrenShortcut);

    connect(ui->quickFilterView, SIGNAL(filterTextChanged(const QString &)), proxy,
            SLOT(setFilterWildcard(const QString &)));
    connect(ui->quickFilterView, SIGNAL(filterClosed()), ui->vTableTreeView, SLOT(setFocus()));

    connect(Core(), SIGNAL(refreshAll()), this, SLOT(refreshVTables()));
}

VTablesWidget::~VTablesWidget()
{
}

void VTablesWidget::refreshVTables()
{
    model->beginReload();
    vtables = CutterCore::getInstance()->getAllVTables();
    model->endReload();

    qhelpers::adjustColumns(ui->vTableTreeView, 3, 0);

    ui->vTableTreeView->setColumnWidth(0, 200);
}

void VTablesWidget::on_vTableTreeView_doubleClicked(const QModelIndex &index)
{
    QModelIndex parent = index.parent();
    if (parent.isValid())
        CutterCore::getInstance()->seek(index.data(
                                            VTableModel::VTableDescriptionRole).value<ClassMethodDescription>().addr);
    else
        CutterCore::getInstance()->seek(index.data(
                                            VTableModel::VTableDescriptionRole).value<VTableDescription>().addr);
}

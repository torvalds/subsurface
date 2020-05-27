#include "desktop-widgets/filterwidget2.h"
#include "desktop-widgets/filterconstraintwidget.h"
#include "desktop-widgets/simplewidgets.h"
#include "desktop-widgets/mainwindow.h"
#include "commands/command.h"
#include "core/qthelper.h"
#include "core/divelist.h"
#include "core/settings/qPrefUnit.h"
#include "core/filterpreset.h"

#include <QDoubleSpinBox>

FilterWidget2::FilterWidget2(QWidget* parent) :
	QWidget(parent),
	ignoreSignal(false),
	validFilter(false)
{
	ui.setupUi(this);

	QMenu *newConstraintMenu = new QMenu(this);
	QStringList constraintTypes = filter_constraint_type_list_translated();
	for (int i = 0; i < constraintTypes.size(); ++i) {
		filter_constraint_type type = filter_constraint_type_from_index(i);
		newConstraintMenu->addAction(constraintTypes[i], [this, type]() { addConstraint(type); });
	}
	ui.addConstraintButton->setMenu(newConstraintMenu);
	ui.addConstraintButton->setPopupMode(QToolButton::InstantPopup);
	ui.constraintTable->setColumnStretch(4, 1); // The fifth column is were the actual constraint resides - stretch that.

	connect(ui.clear, &QToolButton::clicked, this, &FilterWidget2::clearFilter);
	connect(ui.close, &QToolButton::clicked, this, &FilterWidget2::closeFilter);
	connect(ui.fullText, &QLineEdit::textChanged, this, &FilterWidget2::updateFilter);
	connect(ui.fulltextStringMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FilterWidget2::updateFilter);

	connect(&constraintModel, &FilterConstraintModel::rowsInserted, this, &FilterWidget2::constraintAdded);
	connect(&constraintModel, &FilterConstraintModel::rowsRemoved, this, &FilterWidget2::constraintRemoved);
	connect(&constraintModel, &FilterConstraintModel::dataChanged, this, &FilterWidget2::constraintChanged);
	connect(&constraintModel, &FilterConstraintModel::modelReset, this, &FilterWidget2::constraintsReset);

	clearFilter();
}

FilterWidget2::~FilterWidget2()
{
}

void FilterWidget2::constraintAdded(const QModelIndex &parent, int first, int last)
{
	if (parent.isValid() || last < first)
		return; // We only support one level
	constraintWidgets.reserve(constraintWidgets.size() + 1 + last - first);
	for (int i = last + 1; i < (int)constraintWidgets.size(); ++i)
		constraintWidgets[i]->moveToRow(i);
	for (int i = first; i <= last; ++i) {
		QModelIndex idx = constraintModel.index(i, 0);
		constraintWidgets.emplace(constraintWidgets.begin() + i, new FilterConstraintWidget(&constraintModel, idx, ui.constraintTable));
	}
	updateFilter();
}

void FilterWidget2::constraintRemoved(const QModelIndex &parent, int first, int last)
{
	if (parent.isValid() || last < first)
		return; // We only support one level
	constraintWidgets.erase(constraintWidgets.begin() + first, constraintWidgets.begin() + last + 1);
	for (int i = first; i < (int)constraintWidgets.size(); ++i)
		constraintWidgets[i]->moveToRow(i);
	updateFilter();
}

void FilterWidget2::constraintChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
	// Note: this may appear strange, but we don't update the widget if we get
	// a constraint-changed signal from the model. The reason being that the user
	// is currently editing the constraint and we don't want to bother them by
	// overwriting strings with canonicalized data.
	updateFilter();
}

void FilterWidget2::constraintsReset()
{
	constraintWidgets.clear();
	int count = constraintModel.rowCount(QModelIndex());
	for (int i = 0; i < count; ++i) {
		QModelIndex idx = constraintModel.index(i, 0);
		constraintWidgets.emplace_back(new FilterConstraintWidget(&constraintModel, idx, ui.constraintTable));
	}
	updateFilter();
}

void FilterWidget2::clearFilter()
{
	ignoreSignal = true; // Prevent signals to force filter recalculation (TODO: check if necessary)
	ui.fulltextStringMode->setCurrentIndex((int)StringFilterMode::STARTSWITH);
	ui.fullText->clear();
	ignoreSignal = false;
	constraintModel.reload({}); // Causes a filter reload
}

void FilterWidget2::closeFilter()
{
	MainWindow::instance()->setApplicationState(ApplicationState::Default);
}

FilterData FilterWidget2::createFilterData() const
{
	FilterData filterData;
	filterData.fulltextStringMode = (StringFilterMode)ui.fulltextStringMode->currentIndex();
	filterData.fullText = ui.fullText->text();
	filterData.constraints = constraintModel.getConstraints();
	return filterData;
}

void FilterWidget2::updateFilter()
{
	if (ignoreSignal)
		return;

	FilterData filterData = createFilterData();
	validFilter = filterData.validFilter();
	DiveFilter::instance()->setFilter(filterData);
}

void FilterWidget2::on_addSetButton_clicked()
{
	AddFilterPresetDialog dialog(this);
	QString name = dialog.doit();
	if (name.isEmpty())
		return;
	int idx = filter_preset_id(name);
	if (idx >= 0)
		Command::editFilterPreset(idx, createFilterData());
	else
		Command::createFilterPreset(name, createFilterData());
}

void FilterWidget2::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	ui.fullText->setFocus();
	updateFilter();
}

void FilterWidget2::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);
}

void FilterWidget2::addConstraint(filter_constraint_type type)
{
	constraintModel.addConstraint(type);
}

QString FilterWidget2::shownText()
{
	if (validFilter)
		return tr("%L1/%L2 shown").arg(shown_dives).arg(dive_table.nr);
	else
		return tr("%L1 dives").arg(dive_table.nr);
}

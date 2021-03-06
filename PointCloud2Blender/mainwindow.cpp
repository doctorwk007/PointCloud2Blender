/*
    PointCloud2Blender - convert point cloud files to a textured 3D mesh
    Copyright (C) 2015 Adam Kalisz (Bachelor@Kalisz.co)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qRegisterMetaType<Point3D>("Point3D");

    ui->setupUi(this);

    threadPool.setMaxThreadCount(20);
    importer = NULL;
    panorama = NULL;
    mesher = NULL;

    orientation = Panorama3D::RIGHT_UP_Z;
    resolution = 1;
    translation = QVector3D(0,0,0);
    maxDistance = 60.0f;
    projectionType = Panorama3D::EQUIRECTANGULAR;

    originalHorizontalResolution = 0;
    originalVerticalResolution = 0;
    customPanoramaWidth = 0;
    customPanoramaHeight = 0;
    analyzingOriginalResolution = false;

    startTime = QDateTime::currentMSecsSinceEpoch();

    calculateCustomResolution(360*resolution, 180*resolution, 1);

    //Connect gui elements:
    connect(ui->btnFileOpenDialog, SIGNAL(clicked()), this, SLOT(showFileOpenDialog()));
    connect(ui->btnImport, SIGNAL(clicked()), this, SLOT(startFileImport()));
    connect(ui->btnUpVectorLeftX, SIGNAL(clicked()), this, SLOT(onClickUpVectorLeftX()));
    connect(ui->btnUpVectorLeftY, SIGNAL(clicked()), this, SLOT(onClickUpVectorLeftY()));
    connect(ui->btnUpVectorLeftZ, SIGNAL(clicked()), this, SLOT(onClickUpVectorLeftZ()));
    connect(ui->btnUpVectorRightX, SIGNAL(clicked()), this, SLOT(onClickUpVectorRightX()));
    connect(ui->btnUpVectorRightY, SIGNAL(clicked()), this, SLOT(onClickUpVectorRightY()));
    connect(ui->btnUpVectorRightZ, SIGNAL(clicked()), this, SLOT(onClickUpVectorRightZ()));
    connect(ui->btnPanoramaResolutionX1, SIGNAL(clicked()), this, SLOT(onClickPanoramaResolutionX1()));
    connect(ui->btnPanoramaResolutionX2, SIGNAL(clicked()), this, SLOT(onClickPanoramaResolutionX2()));
    connect(ui->btnPanoramaResolutionX4, SIGNAL(clicked()), this, SLOT(onClickPanoramaResolutionX4()));
    connect(ui->btnPanoramaResolutionX8, SIGNAL(clicked()), this, SLOT(onClickPanoramaResolutionX8()));
    connect(ui->btnPanoramaResolutionX16, SIGNAL(clicked()), this, SLOT(onClickPanoramaResolutionX16()));
    connect(ui->sbTranslateX, SIGNAL(valueChanged(double)), this, SLOT(onChangeTranslationVectorX(double)));
    connect(ui->sbTranslateY, SIGNAL(valueChanged(double)), this, SLOT(onChangeTranslationVectorY(double)));
    connect(ui->sbTranslateZ, SIGNAL(valueChanged(double)), this, SLOT(onChangeTranslationVectorZ(double)));
    connect(ui->sbMaxDistance, SIGNAL(valueChanged(double)), this, SLOT(onChangeMaxDistance(double)));
    connect(ui->btnProjectionTypeEquirectangular, SIGNAL(clicked()), this, SLOT(onClickProjectionTypeEquirectangular()));
    connect(ui->btnProjectionTypeCylindrical, SIGNAL(clicked()), this, SLOT(onClickProjectionTypeCylindrical()));
    connect(ui->btnProjectionTypeMercator, SIGNAL(clicked()), this, SLOT(onClickProjectionTypeMercator()));

    connect(ui->sbResolutionHorizontal, SIGNAL(valueChanged(int)), this, SLOT(onChangeResolutionHorizontal(int)));
    connect(ui->sbResolutionVertical, SIGNAL(valueChanged(int)), this, SLOT(onChangeResolutionVertical(int)));
    connect(ui->sbResolutionDivisor, SIGNAL(valueChanged(int)), this, SLOT(onChangeResolutionDivisor(int)));
    connect(ui->btnPanoramaResolutionCustom, SIGNAL(clicked()), this, SLOT(onClickPanoramaResolutionCustom()));
    connect(ui->btnDeterminePanoramaResolution, SIGNAL(clicked()), this, SLOT(onClickDeterminePanoramaResolution()));

    connect(ui->txtFilePathImport, SIGNAL(textChanged(QString)), this, SLOT(onChangeImportPath(QString)));

    generateMenus();
}

MainWindow::~MainWindow()
{
    delete ui;

    //Note: Delete menus?
    if(importer != NULL)
        importer->stopThread();
    if(mesher != NULL)
        mesher->stopThread();

    if(panorama != NULL)
        delete panorama;

    threadPool.waitForDone(30000);
}

void MainWindow::processCommandLine(QString inputFile, QString translation, QString up, int resolution, float distance, QString projection)
{
    qDebug() << "called MainWindow::processCommandLine("<< inputFile <<","<<translation<< ","<<up<<","<<resolution<<","<<distance<<","<<projection<<")";

    setFilePath(inputFile);

    translation.replace("(", "").replace(")", "");
    QStringList translationComponents = translation.split(",");
    if(translationComponents.size() == 3)
    {
        this->translation = QVector3D(translationComponents.at(0).toFloat(),translationComponents.at(1).toFloat(),translationComponents.at(2).toFloat());
    }
    else
    {
        this->translation = QVector3D(0,0,0);
    }

    if(up == "leftx")
    {
        this->orientation = Panorama3D::LEFT_UP_X;
    }
    else if(up == "lefty")
    {
        this->orientation = Panorama3D::LEFT_UP_Y;
    }
    else if(up == "leftz")
    {
        this->orientation = Panorama3D::LEFT_UP_Z;
    }
    else if(up == "rightx")
    {
        this->orientation = Panorama3D::RIGHT_UP_X;
    }
    else if(up == "righty")
    {
        this->orientation = Panorama3D::RIGHT_UP_Y;
    }
    else if(up == "rightz")
    {
        this->orientation = Panorama3D::RIGHT_UP_Z;
    }
    else
    {
        this->orientation = Panorama3D::RIGHT_UP_Z;
    }

    this->resolution = resolution;
    this->maxDistance = distance;
    if(projection == "equirectangular")
    {
        this->projectionType = Panorama3D::EQUIRECTANGULAR;
    }
    else if(projection == "cylindrical")
    {
        this->projectionType = Panorama3D::CYLINDRICAL;
    }
    else if(projection == "mercator")
    {
        this->projectionType = Panorama3D::MERCATOR;
    }
    else
    {
        this->projectionType = Panorama3D::EQUIRECTANGULAR;
    }

    startFileImport();


}

void MainWindow::generateMenus()
{
    //Load application settings
    QString recentPath = settings.value("paths/recentPath", "").toString();

    //Generate menus
    menuFile = new QMenu("&File", this);
    actOpen = new QAction("&Open...", this);
    actOpen->setShortcut(QKeySequence::Open);
    actOpen->setStatusTip( tr("Open a new point cloud file") );
    connect(actOpen, SIGNAL(triggered()), this, SLOT(showFileOpenDialog()));
    if(!recentPath.isEmpty())
    {
        menuOpenRecent = new QMenu("Open &recent...", menuFile);
        actOpenRecent = new QAction(recentPath, this);
        connect(actOpenRecent, SIGNAL(triggered()), this, SLOT(openRecentFile()));
    }
    actExit  = new QAction("&Exit", this);
    connect(actExit, SIGNAL(triggered()), this, SLOT(close()));

    //Add menus
    ui->menuBar->addMenu(menuFile);
    menuFile->addAction(actOpen);
    if(!recentPath.isEmpty())
    {
        menuFile->addMenu(menuOpenRecent);
        menuOpenRecent->addAction(actOpenRecent);
    }
    menuFile->addSeparator();
    menuFile->addAction(actExit);

}

void MainWindow::calculateCustomResolution(int horizontal, int vertical, int divisor)
{
    customPanoramaWidth = horizontal / divisor;
    customPanoramaHeight = vertical / divisor;
    ui->lblPanoramaResolutionCustomFinal->setText("= (" + QString::number(customPanoramaWidth) + " x " + QString::number(customPanoramaHeight) + ")");
}

void MainWindow::showFileOpenDialog()
{
    //The following filetypes are selectable
    QString fileFormat = "All Files (*.*);;XYZ Ascii Files (*xyz);;XYZ Binary Files (*xyb);;PLY Ascii Files (*ply)";
    QString fileName = "";

    fileName = QFileDialog::getOpenFileName(this, "Please specify your point cloud file", QDir::currentPath() + "/../TestData", fileFormat);

    setFilePath(fileName);
}

void MainWindow::setFilePath(QString fileName)
{
    //Save recent Path as setting
    settings.setValue("paths/recentPath", fileName);

    if(!fileName.isEmpty())
    {
        ui->txtFilePathImport->setText(fileName);
        ui->btnImport->setEnabled(true);
    }
    else
    {
        ui->btnImport->setEnabled(false);
    }
}

void MainWindow::startFileImport()
{
    startTime = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "MainWindow::startFileImport()";

    ui->canvasGL->pointCloudMesh->reset(false);

    ui->btnDeterminePanoramaResolution->setEnabled(false);
    ui->btnImport->setEnabled(false);

    setStatusTip("Importing...");

    //delete panorama
    if(panorama != NULL)
    {
        delete panorama;
    }
    panorama = new Panorama3D(translation, orientation, customPanoramaWidth, customPanoramaHeight, maxDistance, projectionType, this);
    connect(panorama, SIGNAL(updateDepthMap(QImage*)), this, SLOT(updateDepthMap(QImage*)), Qt::QueuedConnection);
    connect(panorama, SIGNAL(updateColorMap(QImage*)), this, SLOT(updateColorMap(QImage*)), Qt::QueuedConnection);

    //delete importer
    importer = new ImportWorker(panorama, ui->canvasGL, ui->txtFilePathImport->text(), false);
    connect(importer, SIGNAL(importStatus(float)), this, SLOT(updateImportStatus(float)));
    connect(importer, SIGNAL(showInfoMessage(QString)), this, SLOT(showInfoMessage(QString)));
    connect(importer, SIGNAL(showErrorMessage(QString)), this, SLOT(showErrorMessage(QString)));

    threadPool.start(importer);
}

void MainWindow::openRecentFile()
{
    //Get the text from the menu item and use it as the filepath
    QAction *action = qobject_cast<QAction*>(sender());
    if(action)
    {
        setFilePath(action->text());
        startFileImport();
    }
}

void MainWindow::updateImportStatus(float percent)
{
    ui->prbImportStatus->setValue(percent);

    if(percent >= 100.0f)
    {
        if(analyzingOriginalResolution)
        {
            analyzingOriginalResolution = false;
            ui->btnDeterminePanoramaResolution->setEnabled(true);
            ui->btnImport->setEnabled(true);
        }
        else
        {
            ui->prbImportStatus->setValue(0);
            setStatusTip("Saving panoramas...");
            panorama->finished();
            ui->canvasGL->pointCloudMesh->finished();

            setStatusTip("Meshing...");
            mesher = new MeshWorker(panorama, ui->canvasGL, ui->sbNormalAngle->value(), this);
            connect(mesher, SIGNAL(meshingStatus(float)), this, SLOT(updateMeshingStatus(float)));
            threadPool.start(mesher);
        }

    }
}

void MainWindow::updateMeshingStatus(float percent)
{
    ui->prbImportStatus->setValue(percent);

    float duration = (QDateTime::currentMSecsSinceEpoch() - startTime) / 1000.0f;
    float minutes = duration / 60.0f;
    ui->prbImportStatus->setFormat("%p%");

    if(percent >= 100.0f)
    {
        QMessageBox::information(this, "Meshing complete!", "The meshing process has finished. This took " + QString::number(minutes, 'f', 2) + " minutes.\n\nYou can see the result in the 3D viewer and you can use the generated .obj file in Blender. Have fun!", QMessageBox::Ok);
        ui->btnImport->setEnabled(true);
    }
}

void MainWindow::setOriginalResolution(int horizontalResolution)
{
    originalHorizontalResolution = horizontalResolution;
    originalVerticalResolution = horizontalResolution / 2;

    ui->lblOriginalHorizontalResolution->setText( QString::number(originalHorizontalResolution) );
    ui->lblOriginalVerticalResolution->setText( QString::number(originalVerticalResolution) );

    ui->sbResolutionHorizontal->setValue(originalHorizontalResolution);
    ui->sbResolutionVertical->setValue(originalVerticalResolution);

    //Save as App-setting
    settings.setValue("import/originalHorizontalResolution", originalHorizontalResolution);

    //Recalculate resolution
    this->calculateCustomResolution(originalHorizontalResolution, originalVerticalResolution, 1);

    ui->btnPanoramaResolutionCustom->setChecked(true);
    //ui->btnImport->setEnabled(true);

    float duration = (QDateTime::currentMSecsSinceEpoch() - startTime) / 1000.0f;
    float minutes = duration / 60.0f;

    QMessageBox::information(this, "File analyzed!", "Analyzing the file took " + QString::number(minutes, 'f', 2) + " minutes.\nThe original resolution of the point cloud probably was:\n" + QString::number(originalHorizontalResolution) + " by " + QString::number(originalVerticalResolution) +" Points.", QMessageBox::Ok);

}

void MainWindow::updateDepthMap(QImage *depthMap)
{
    ui->lblPanoramaDepth->setPixmap( QPixmap::fromImage(*depthMap) );
    this->repaint();
}

void MainWindow::updateColorMap(QImage *colorMap)
{
    ui->lblPanoramaColor->setPixmap( QPixmap::fromImage(*colorMap) );
    this->repaint();
}

void MainWindow::onClickUpVectorLeftX()
{
    orientation = Panorama3D::LEFT_UP_X;
}

void MainWindow::onClickUpVectorLeftY()
{
    orientation = Panorama3D::LEFT_UP_Y;
}

void MainWindow::onClickUpVectorLeftZ()
{
    orientation = Panorama3D::LEFT_UP_Z;
}

void MainWindow::onClickUpVectorRightX()
{
    orientation = Panorama3D::RIGHT_UP_X;
}

void MainWindow::onClickUpVectorRightY()
{
    orientation = Panorama3D::RIGHT_UP_Y;
}

void MainWindow::onClickUpVectorRightZ()
{
    orientation = Panorama3D::RIGHT_UP_Z;
}

void MainWindow::onClickPanoramaResolutionX1()
{
    resolution = 1;
    calculateCustomResolution(360*resolution, 180*resolution, 1);
}

void MainWindow::onClickPanoramaResolutionX2()
{
    resolution = 2;
    calculateCustomResolution(360*resolution, 180*resolution, 1);
}

void MainWindow::onClickPanoramaResolutionX4()
{
    resolution = 4;
    calculateCustomResolution(360*resolution, 180*resolution, 1);
}

void MainWindow::onClickPanoramaResolutionX8()
{
    resolution = 8;
    calculateCustomResolution(360*resolution, 180*resolution, 1);
}

void MainWindow::onClickPanoramaResolutionX16()
{
    resolution = 16;
    calculateCustomResolution(360*resolution, 180*resolution, 1);
}

void MainWindow::onChangeTranslationVectorX(double x)
{
    translation.setX(x);
}

void MainWindow::onChangeTranslationVectorY(double y)
{
    translation.setY(y);
}

void MainWindow::onChangeTranslationVectorZ(double z)
{
    translation.setZ(z);
}

void MainWindow::onChangeMaxDistance(double distance)
{
    this->maxDistance = distance;
}

void MainWindow::onClickProjectionTypeEquirectangular()
{
    this->projectionType = Panorama3D::EQUIRECTANGULAR;
}

void MainWindow::onClickProjectionTypeCylindrical()
{
    this->projectionType = Panorama3D::CYLINDRICAL;
}

void MainWindow::onClickProjectionTypeMercator()
{
    this->projectionType = Panorama3D::MERCATOR;
}

void MainWindow::onChangeResolutionHorizontal(int value)
{
    calculateCustomResolution(value, ui->sbResolutionVertical->value(), ui->sbResolutionDivisor->value());
}

void MainWindow::onChangeResolutionVertical(int value)
{
    calculateCustomResolution(ui->sbResolutionHorizontal->value(), value, ui->sbResolutionDivisor->value());
}

void MainWindow::onChangeResolutionDivisor(int value)
{
    calculateCustomResolution(ui->sbResolutionHorizontal->value(), ui->sbResolutionVertical->value(), value);
}

void MainWindow::onClickPanoramaResolutionCustom()
{
    calculateCustomResolution(ui->sbResolutionHorizontal->value(), ui->sbResolutionVertical->value(), ui->sbResolutionDivisor->value());
}

void MainWindow::onClickDeterminePanoramaResolution()
{
    startTime = QDateTime::currentMSecsSinceEpoch();

    analyzingOriginalResolution = true;
    ui->btnDeterminePanoramaResolution->setEnabled(false);
    ui->btnImport->setEnabled(false);

    if(this->panorama != NULL)
    {
        delete this->panorama;
    }
    this->panorama = new Panorama3D(translation, orientation, customPanoramaWidth, customPanoramaHeight, maxDistance, projectionType, this);
    this->importer = new ImportWorker(this->panorama, ui->canvasGL, ui->txtFilePathImport->text(), true, this);
    connect(this->importer, SIGNAL(originalResolution(int)), this, SLOT(setOriginalResolution(int)));
    connect(this->importer, SIGNAL(importStatus(float)), this, SLOT(updateImportStatus(float)));

    ui->lblOriginalHorizontalResolution->setText("analyzing...");
    ui->lblOriginalVerticalResolution->setText("analyzing...");

    threadPool.start(this->importer);
}

void MainWindow::onChangeImportPath(QString path)
{
    QFile checkFile(path);

    if(!path.isEmpty() && checkFile.exists())
    {
        ui->btnDeterminePanoramaResolution->setEnabled(true);
    }
    else
    {
        ui->btnDeterminePanoramaResolution->setEnabled(false);
    }
}

void MainWindow::onClickExportPanoramas()
{
    //TODO
}

void MainWindow::onClickExportMesh()
{
    //TODO
}

void MainWindow::showInfoMessage(QString message)
{
    QMessageBox::information(this, "Information", message);
}

void MainWindow::showErrorMessage(QString message)
{
    QMessageBox::critical(this, "Error", message);
}

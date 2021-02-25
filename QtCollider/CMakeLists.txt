set(REQUIRED_QT_VERSION 5.7)
option(SC_USE_QTWEBENGINE "Build with Qt WebEngine." ON)

###############################################################################
# Components and libraries

set(
  QT_COMPONENTS
  Core
  Widgets
  Network
  PrintSupport
  OpenGL
  Quick
  Qml
  Sql
  Svg
)

set (QT_COLLIDER_LIBS
  Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::PrintSupport
  Qt5::Quick Qt5::Qml Qt5::Sql Qt5::OpenGL Qt5::Svg
  ${MATH_LIBRARY})

if(SC_USE_QTWEBENGINE)
  list(APPEND QT_COMPONENTS WebEngine WebEngineCore WebEngineWidgets)
  list(APPEND QT_COLLIDER_LIBS Qt5::WebEngineCore Qt5::WebEngineWidgets)
endif()

find_package(Qt5 ${REQUIRED_QT_VERSION} COMPONENTS ${QT_COMPONENTS} REQUIRED)

if (APPLE)
  find_package(Qt5MacExtras)
  mark_as_advanced(Qt5MacExtras_DIR)
  list(APPEND QT_COLLIDER_LIBS Qt5::MacExtras)
endif()

###############################################################################
# Debug flags

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}  -DQC_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}  -DQC_DEBUG")

###############################################################################
# Source file manifest

set(QT_COLLIDER_DIR ${CMAKE_SOURCE_DIR}/QtCollider)

include_directories( ${QT_COLLIDER_DIR} )

set( QT_COLLIDER_HDRS
  ${QT_COLLIDER_DIR}/QcApplication.h
  ${QT_COLLIDER_DIR}/QObjectProxy.h
  ${QT_COLLIDER_DIR}/QWidgetProxy.h
  ${QT_COLLIDER_DIR}/QtDownload.h
  ${QT_COLLIDER_DIR}/QcCallback.hpp
  ${QT_COLLIDER_DIR}/widgets/BasicWidgets.h
  ${QT_COLLIDER_DIR}/widgets/QcButton.h
  ${QT_COLLIDER_DIR}/widgets/QcCheckBox.h
  ${QT_COLLIDER_DIR}/widgets/QcTextField.h
  ${QT_COLLIDER_DIR}/widgets/QcListWidget.h
  ${QT_COLLIDER_DIR}/widgets/QcPopUpMenu.h
  ${QT_COLLIDER_DIR}/widgets/QcTreeWidget.h
  ${QT_COLLIDER_DIR}/widgets/QcMenu.h
  ${QT_COLLIDER_DIR}/widgets/QcScrollArea.h
  ${QT_COLLIDER_DIR}/widgets/QcNumberBox.h
  ${QT_COLLIDER_DIR}/widgets/QcSlider.h
  ${QT_COLLIDER_DIR}/widgets/QcSlider2D.h
  ${QT_COLLIDER_DIR}/widgets/QcRangeSlider.h
  ${QT_COLLIDER_DIR}/widgets/QcMultiSlider.h
  ${QT_COLLIDER_DIR}/widgets/QcKnob.hpp
  ${QT_COLLIDER_DIR}/widgets/QcTextEdit.h
  ${QT_COLLIDER_DIR}/widgets/QcScope.h
  ${QT_COLLIDER_DIR}/widgets/QcScopeShm.h
  ${QT_COLLIDER_DIR}/widgets/QcGraph.h
  ${QT_COLLIDER_DIR}/widgets/QcLevelIndicator.h
  ${QT_COLLIDER_DIR}/widgets/QcCanvas.h
  ${QT_COLLIDER_DIR}/widgets/soundfileview/view.hpp
  ${QT_COLLIDER_DIR}/widgets/QcFileDialog.h
  ${QT_COLLIDER_DIR}/widgets/QcPenPrinter.h
  ${QT_COLLIDER_DIR}/widgets/QcWindow.h
  ${QT_COLLIDER_DIR}/layouts/layouts.hpp
  ${QT_COLLIDER_DIR}/layouts/stack_layout.hpp
  ${QT_COLLIDER_DIR}/style/style.hpp
)

set( QT_COLLIDER_SRCS
  ${QT_COLLIDER_DIR}/widgets/soundfileview/view.cpp
)

if(QT_COLLIDER_LANG_CLIENT)
    list(APPEND QT_COLLIDER_HDRS ${QT_COLLIDER_DIR}/LanguageClient.h)
    list(APPEND QT_COLLIDER_SRCS ${QT_COLLIDER_DIR}/LanguageClient.cpp)
endif()

set(QT_COLLIDER_WEBENGINE_HDRS
  ${QT_COLLIDER_DIR}/widgets/QcWebView.h
  ${QT_COLLIDER_DIR}/widgets/web_page.hpp
)

if(SC_USE_QTWEBENGINE)
  list(APPEND QT_COLLIDER_HDRS ${QT_COLLIDER_WEBENGINE_HDRS})
endif()

###############################################################################

qt5_wrap_cpp( QT_COLLIDER_MOC_SRCS ${QT_COLLIDER_HDRS} )
qt5_add_resources( QT_COLLIDER_RCC ${QT_COLLIDER_DIR}/resources.qrc )

if(APPLE)
	qt5_wrap_cpp( QT_COLLIDER_OBJ_MOC_SRCS ${QT_COLLIDER_DIR}/widgets/QcQuartzComposerView.h )
	list(APPEND QT_COLLIDER_HDRS ${QT_COLLIDER_DIR}/widgets/QcQuartzComposerView.h )
  	set( QT_COLLIDER_OBJCPP_SRCS
  		${QT_COLLIDER_OBJ_MOC_SRCS}
		${QT_COLLIDER_DIR}/hacks/hacks_mac.mm
		${QT_COLLIDER_DIR}/widgets/QcQuartzComposerView.mm
		${QT_COLLIDER_DIR}/primitives/prim_QQuartzComposer.mm
  	)
endif()

set_source_files_properties( ${QT_COLLIDER_OBJCPP_SRCS}
    PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-exceptions"
)

list( APPEND QT_COLLIDER_SRCS
  ${QT_COLLIDER_OBJCPP_SRCS}
  ${QT_COLLIDER_MOC_SRCS}
  ${QT_COLLIDER_RCC}
  ${QT_COLLIDER_WEBENGINE_HDRS}
)

add_definitions(-DSC_QT -DQT_COLLIDER_EXPORTING -DQT_NO_KEYWORDS)

if(APPLE)
  list(APPEND QT_COLLIDER_LIBS "-framework Cocoa" )
  list(APPEND QT_COLLIDER_LIBS "-framework Quartz" )
  list(APPEND QT_COLLIDER_LIBS "-framework AppKit" )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -F/Library/Frameworks")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_package(X11 REQUIRED)
    include_directories(X11_INCLUDE_DIR)
    list(APPEND QT_COLLIDER_LIBS ${X11_X11_LIB})
endif()

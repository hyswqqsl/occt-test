#include "OcctWindow.h"

// native API 暴露 + 规避 macOS Cocoa 头与 OCCT Handle 宏的名字冲突
#if defined (__APPLE__)
  #undef Handle
  #define GLFW_EXPOSE_NATIVE_COCOA
  #define GLFW_EXPOSE_NATIVE_NSGL
#elif defined (_WIN32)
  #define GLFW_EXPOSE_NATIVE_WIN32
  #define GLFW_EXPOSE_NATIVE_WGL
#else
  #define GLFW_EXPOSE_NATIVE_X11
  #define GLFW_EXPOSE_NATIVE_GLX
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

OcctWindow::OcctWindow (int theWidth, int theHeight, const TCollection_AsciiString& theTitle)
: myGlfwWindow (glfwCreateWindow (theWidth, theHeight, theTitle.ToCString(), NULL, NULL)),
  myXLeft (0), myYTop (0), myXRight (0), myYBottom (0)
{
  if (myGlfwWindow != nullptr)
  {
    int aWidth = 0, aHeight = 0;
    glfwGetWindowPos (myGlfwWindow, &myXLeft, &myYTop);
    glfwGetWindowSize (myGlfwWindow, &aWidth, &aHeight);
    myXRight  = myXLeft + aWidth;
    myYBottom = myYTop + aHeight;
  #if !defined(_WIN32) && !defined(__APPLE__)
    myDisplay = new Aspect_DisplayConnection ((Aspect_XDisplay* )glfwGetX11Display());
  #endif
  }
}

void OcctWindow::Close()
{
  if (myGlfwWindow != nullptr)
  {
    glfwDestroyWindow (myGlfwWindow);
    myGlfwWindow = nullptr;
  }
}

Aspect_Drawable OcctWindow::NativeHandle() const
{
#if defined (__APPLE__)
  return (Aspect_Drawable )glfwGetCocoaWindow (myGlfwWindow);
#elif defined (_WIN32)
  return (Aspect_Drawable )glfwGetWin32Window (myGlfwWindow);
#else
  return (Aspect_Drawable )glfwGetX11Window (myGlfwWindow);
#endif
}

Aspect_RenderingContext OcctWindow::NativeGlContext() const
{
#if defined (__APPLE__)
  // GLFW 3.4 起 C++ 模式下不声明 AppKit 类型, 返回 void*, OCCT 只需要指针值
  return (Aspect_RenderingContext )glfwGetNSGLContext (myGlfwWindow);
#elif defined (_WIN32)
  return (Aspect_RenderingContext )glfwGetWGLContext (myGlfwWindow);
#else
  return (Aspect_RenderingContext )glfwGetGLXContext (myGlfwWindow);
#endif
}

Standard_Boolean OcctWindow::IsMapped() const
{
  return glfwGetWindowAttrib (myGlfwWindow, GLFW_VISIBLE) != 0;
}

void OcctWindow::Map() const
{
  glfwShowWindow (myGlfwWindow);
  glfwSwapBuffers (myGlfwWindow);
}

void OcctWindow::Unmap() const
{
  glfwHideWindow (myGlfwWindow);
}

Aspect_TypeOfResize OcctWindow::DoResize()
{
  if (glfwGetWindowAttrib (myGlfwWindow, GLFW_VISIBLE) == 1)
  {
    int anXPos = 0, anYPos = 0, aWidth = 0, aHeight = 0;
    glfwGetWindowPos (myGlfwWindow, &anXPos, &anYPos);
    glfwGetWindowSize (myGlfwWindow, &aWidth, &aHeight);
    myXLeft   = anXPos;
    myYTop    = anYPos;
    myXRight  = anXPos + aWidth;
    myYBottom = anYPos + aHeight;
  }
  return Aspect_TOR_UNKNOWN;
}

Graphic3d_Vec2i OcctWindow::CursorPosition() const
{
  Graphic3d_Vec2d aPos;
  glfwGetCursorPos (myGlfwWindow, &aPos.x(), &aPos.y());
  return Graphic3d_Vec2i ((int )aPos.x(), (int )aPos.y());
}

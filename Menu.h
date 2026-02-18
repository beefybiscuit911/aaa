#pragma once
// #include "Interfaces.h" // Removed as it is missing

template <typename T> class Singleton {
public:
  static T &Get() {
    static T instance;
    return instance;
  }
};

class Menu : public Singleton<Menu> {

public:
  void Render();
  void Shutdown();
  void ColorPicker(const char *name, float *color, bool alpha);

  void Legit();
  void Aimbot();
  void Antiaim();
  void Visuals();
  void Misc();
  void Skins();
  void Players();

  bool isOpen = true; // Default to true for testing
};
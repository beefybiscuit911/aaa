#pragma once
#include "includes.h"

#include "keys.hpp"
#include "screen.hpp"

inline void promptConfigModification() {
  system("cls");

start_target_color:
  std::cout
      << "Target color:" << std::endl
      << "- Current value " << config.targetColor << std::endl
      << "- Available options (purple, yellow, skip (to leave it as it is))"
      << std::endl;
  std::string response;
  std::cout << "Select your target color: " << std::endl;
  std::cin >> response;

  if (response == "skip") {
    system("cls");
    goto start_tolerance;
  } else if (response == "purple" || response == "yellow")
    config.targetColor = response;
  else {
    system("cls");
    std::cout << "Invalid input, check options" << std::endl << std::endl;
    goto start_target_color;
  }

  system("cls");
start_tolerance:
  std::cout << "Tolerance (color sensitivity):" << std::endl
            << "- Current value " << config.tolerance << std::endl
            << "Options: Enter the tolerance number or type \"skip\" to leave "
               "it as it is"
            << std::endl;
  std::cout << "Enter your tolerance: " << std::endl;
  std::cin >> response;

  if (response == "skip") {
    system("cls");
    goto start_fov_x;
  } else {
    int tol = atoi(response.c_str());
    if (tol == 0) {
      system("cls");
      std::cout << "Invalid input, check options" << std::endl << std::endl;
      goto start_tolerance;
    }

    config.tolerance = tol;
  }

  system("cls");
start_fov_x:
  std::cout << "Scan area X:" << std::endl
            << "- Current value " << config.scanAreaX << std::endl
            << "Options: Enter the scan area X number or type \"skip\" to "
               "leave it as it is"
            << std::endl;
  std::cout << "Enter your scan area X: " << std::endl;
  std::cin >> response;

  if (response == "skip") {
    goto start_fov_y;
  } else {
    int fovX = atoi(response.c_str());
    if (fovX == 0) {
      system("cls");
      std::cout << "Invalid input, check options" << std::endl << std::endl;
      goto start_fov_x;
    }

    config.scanAreaX = fovX;
  }

  system("cls");
start_fov_y:
  std::cout << "Scan area Y:" << std::endl
            << "- Current value " << config.scanAreaY << std::endl
            << "Options: Enter the scan area Y number or type \"skip\" to "
               "leave it as it is"
            << std::endl;
  std::cout << "Enter your scan area Y: " << std::endl;
  std::cin >> response;

  if (response == "skip") {
    system("cls");
    goto start_gravity;
  } else {
    int fovY = atoi(response.c_str());
    if (fovY == 0) {
      system("cls");
      std::cout << "Invalid input, check options" << std::endl << std::endl;
      goto start_fov_y;
    }

    config.scanAreaY = fovY;
  }

  system("cls");
start_gravity:
  std::cout << "Gravity (mouse lock power):" << std::endl
            << "- Current value " << config.gravity << std::endl
            << "Options: Enter the mouse gravity or type \"skip\" to leave it "
               "as it is"
            << std::endl;
  std::cout << "Enter your mouse gravity: " << std::endl;
  std::cin >> response;

  if (response == "skip")
    goto start_toggle_key;
  else {
    int gravity = atoi(response.c_str());
    if (gravity == 0) {
      system("cls");
      std::cout << "Invalid input, check options" << std::endl << std::endl;
      goto start_gravity;
    }

    config.gravity = gravity;
  }

  system("cls");
start_toggle_key:
  std::cout
      << "Toggle key:" << std::endl
      << "- Current value " << config.toggleKey << std::endl
      << "- Available options (" << std::endl
      << "left_mouse_button, right_mouse_button, x1, x2," << std::endl
      << "num_0, num_1, num_2, num_3, num_4, num_5, num_6, num_7, num_8, num_9,"
      << std::endl
      << "a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, "
         "x, y, z,"
      << std::endl
      << "up_arrow, down_arrow, enter, esc, home, insert, left_alt, left_ctrl, "
         "left_shift, "
      << std::endl
      << "page_down, page_up, right_alt, right_ctrl, right_shift, space, tab, "
         "backspace,"
      << std::endl
      << "f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12" << std::endl
      << "skip (to leave it as it is))" << std::endl;
  std::cout << "Select your toggle key: " << std::endl;
  std::cin >> response;

  if (response == "skip") {
    system("cls");
    goto start_fps;
  } else {
    if (!KeyUtils::isValidKey(response)) {
      system("cls");
      std::cout << "Invalid input, check options" << std::endl << std::endl;
      goto start_toggle_key;
    }
    config.toggleKey = response;
  }

  system("cls");
start_fps:
  std::cout
      << "Refresh rate (FPS):" << std::endl
      << "- Current value " << config.refreshRate << "FPS" << std::endl
      << "Options: Enter the refresh rate or type \"skip\" to leave it as it is"
      << std::endl;
  std::cout << "Enter your prefered refresh rate: " << std::endl;
  std::cin >> response;

  if (response == "skip")
    return;
  else {
    int fps = atoi(response.c_str());
    if (fps == 0) {
      system("cls");
      std::cout << "Invalid input, check options" << std::endl << std::endl;
      goto start_fps;
    }

    config.refreshRate = fps;
  }
}

inline void startupRoutine() {
  // checking for already saved config

  loadConfig();
  bool loadConfigFile = true;

  if (!loadConfigFile)
    std::cout << "No config file was found! " << std::endl
              << "Press Y to create a new custom config or N to stick to the "
                 "default settings: ";
  else
    std::cout << "Loaded saved config successfully, do you want to modify it? "
                 "(Y/N): ";

  char response;
  std::cin >> response;

  if (response == 'Y' || response == 'y') {
    promptConfigModification();
  }

  saveConfig();
}
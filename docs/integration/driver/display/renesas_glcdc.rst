.. _renesas_glcdc:

=============
Renesas GLCDC
=============

Overview
--------

.. image:: /misc/renesas/glcdc.png
    :alt: Architectural overview of Renesas GLCDC
    :align: center

|

GLCDC is a multi-stage graphics output peripheral used in Renesas MCUs.
It is designed to automatically generate timing and data signals for different LCD panels.

- Supports LCD panels with RGB interface (up to 24 bits) and sync signals (HSYNC, VSYNC and Data Enable optional)
- Supports various color formats for input graphics planes (RGB888, ARGB8888, RGB565, ARGB1555, ARGB4444, CLUT8, CLUT4, CLUT1)
- Supports the Color Look-Up Table (CLUT) usage for input graphics planes (ARGB8888) with 512 words (32 bits/word)
- Supports various color formats for output (RGB888, RGB666, RGB565, Serial RGB888)
- Can input two graphics planes on top of the background plane and blend them on the screen
- Generates a dot clock to the panel. The clock source is selectable from internal or external (LCD_EXTCLK)
- Supports brightness adjustment, contrast adjustment, and gamma correction
- Supports GLCDC interrupts to handle frame-buffer switching or underflow detection

| Setting up a project and further integration with Renesas' ecosystem is described in detail on :ref:`page Renesas <renesas>`.
| Check out the `EK-RA8D1 example repository <https://github.com/lvgl/lv_port_renesas_ek-ra8d1>`__ for a ready-to-use example.

Prerequisites
-------------

- This diver relies on FSP generated code. Missing the step while setting up the project will cause a compilation error.
- Activate the diver by setting :c:macro:`LV_USE_DRAW_PXP` to ``1`` in your *"lv_conf.h"*.

Usage
-----

There is no need to implement any platform-specific functions.

The following code demonstrates using the diver in :cpp:enumerator:`LV_DISPLAY_RENDER_MODE_DIRECT` mode.

.. code:: c

    lv_display_t * disp = lv_renesas_glcdc_direct_create();
    lv_display_set_default(disp);

To use the driver in :cpp:enumerator:`LV_DISPLAY_RENDER_MODE_PARTIAL` mode, an extra buffer must be allocated,
desireably in the fastest available memory region.
Buffer swapping can be activated by passing a second buffer of same size insted of the :cpp:expr:`NULL` argument.

.. code:: c

    static lv_color_t partial_draw_buf[DISPLAY_HSIZE_INPUT0 * DISPLAY_VSIZE_INPUT0 / 10] BSP_PLACE_IN_SECTION(".sdram") BSP_ALIGN_VARIABLE(1024);

    lv_display_t * disp = lv_renesas_glcdc_partial_create(partial_draw_buf, NULL, sizeof(partial_draw_buf));
    lv_display_set_default(disp);

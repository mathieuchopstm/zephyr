.. zephyr:code-sample:: hello_world
   :name: Hello World

.. code-block:: shell
    $ west build samples/hello_world/ -b mps2/an521/cpu0  -p
    $ west build -t run
    ... observe failure ...
    $ west build samples/hello_world/ -b mps2/an521/cpu0 -DCONFIG_ARM_MPU_HACK_RECONFIGURE_STATIC_REGIONS=n -p
    $ west build -t run

Exit QEMU by pressing :kbd:`CTRL+A` :kbd:`x` after each ``west build -t run``.

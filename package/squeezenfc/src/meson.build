project('squeezenfc', ['c'],
        version : '3.0',
        license : 'GPLv3',
        default_options: [
                'c_std=gnu99',
                'prefix=/usr',
                'sysconfdir=/etc',
                'localstatedir=/var',
                'buildtype=debug'
        ],
        meson_version : '>= 0.41',
       )

dep = dependency('libcurl libnfc libcjson')

sources = files('''
    main.c
'''.split())

squeezenfc = executable(
      'squeezenfc',
      [sources],
      dependencies : [dep],
      install : true)



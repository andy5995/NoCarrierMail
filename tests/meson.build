# Each test is a standalone executable linked only against the specific
# self-contained source under test, then registered with meson's test().
# Source file lists that reach outside this directory are passed in from
# the top-level meson.build (meson forbids files() from escaping the subdir).

tests = [
    {'name': 'misc', 'extra': mm_misc_src},
    {'name': 'qwkhdr', 'extra': mm_qwkhdr_src + mm_misc_src},
    {'name': 'qwkvote', 'extra': mm_qwkvote_src + mm_qwkhdr_src + mm_misc_src},
    {'name': 'qheader', 'extra': mm_qheader_src + mm_misc_src},
]

foreach t : tests
    test(
        t['name'],
        executable(
            'test_' + t['name'],
            files(t['name'] + '_test.cc') + t['extra'],
            include_directories: include_directories('..'),
            override_options: cpp98,
        ),
    )
endforeach

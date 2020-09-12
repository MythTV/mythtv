contains(QMAKE_CXX, "g++") {
  !contains(QMAKE_CXXFLAGS, "-O2"): QMAKE_CXXFLAGS += -O0
  QMAKE_CXXFLAGS += -fprofile-arcs -ftest-coverage
  QMAKE_LFLAGS += -fprofile-arcs
}

QMAKE_LFLAGS += -Wl,$$_RPATH_$(PWD)/../..

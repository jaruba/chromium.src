# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
from measurements import thread_times
import page_sets
from telemetry import benchmark

class _ThreadTimes(benchmark.Benchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--report-silk-details', action='store_true',
                      help='Report details relevant to silk.')

  def CreatePageTest(self, options):
    return thread_times.ThreadTimes(options.report_silk_details)


@benchmark.Enabled('android')
class ThreadTimesKeySilkCases(_ThreadTimes):
  """Measures timeline metrics while performing smoothness action on key silk
  cases."""
  page_set = page_sets.KeySilkCasesPageSet

@benchmark.Enabled('android', 'linux')
class ThreadTimesKeyHitTestCases(_ThreadTimes):
  """Measure timeline metrics while performing smoothness action on key hit
  testing cases."""
  page_set = page_sets.KeyHitTestCasesPageSet

@benchmark.Disabled
class LegacySilkBenchmark(ThreadTimesKeySilkCases):
  """Same as thread_times.key_silk_cases but with the old name."""
  @classmethod
  def Name(cls):
    return "silk.key_silk_cases"


@benchmark.Enabled('android')
class ThreadTimesFastPathMobileSites(_ThreadTimes):
  """Measures timeline metrics while performing smoothness action on
  key mobile sites labeled with fast-path tag.
  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  options = {'page_label_filter' : 'fastpath'}


@benchmark.Enabled('android')
class ThreadTimesSimpleMobileSites(_ThreadTimes):
  """Measures timeline metric using smoothness action on simple mobile sites
  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  page_set = page_sets.SimpleMobileSitesPageSet


@benchmark.Disabled('win') # crbug.com/443781
class ThreadTimesCompositorCases(_ThreadTimes):
  """Measures timeline metrics while performing smoothness action on
  tough compositor cases, using software rasterization.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  page_set = page_sets.ToughCompositorCasesPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForSoftwareRasterization(options)

@benchmark.Enabled('android')
class ThreadTimesPolymer(_ThreadTimes):
  """Measures timeline metrics while performing smoothness action on
  Polymer cases."""
  page_set = page_sets.PolymerPageSet

# -*- coding: UTF-8 -*-

#  Copyright (c) 2020 Lachlan Mackenzie
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.


from __future__ import unicode_literals


API_URL = 'https://api.tvmaze.com'
search_show_name = 'https://api.tvmaze.com/search/shows?'
search_show_best_match = 'https://api.tvmaze.com/singlesearch/shows?'
search_external_show_id = 'https://api.tvmaze.com/lookup/shows?'
show_information = 'https://api.tvmaze.com/shows/{0}'
show_episode_list = 'https://api.tvmaze.com/shows/{0}/episodes?'
show_episode = 'http://api.tvmaze.com/shows/{0}/episodebynumber?'
show_episodes_on_date = 'http://api.tvmaze.com/shows/{0}/episodesbydate?'
show_season_list = 'http://api.tvmaze.com/shows/{0}/seasons'
season_episode_list = 'http://api.tvmaze.com/seasons/{0}/episodes'
show_alias_list = 'http://api.tvmaze.com/shows/{0}/akas'
episode_information = 'http://api.tvmaze.com/episodes/{0}?'
show_cast = 'http://api.tvmaze.com/shows/{0}/cast'
show_crew = 'http://api.tvmaze.com/shows/{0}/crew'

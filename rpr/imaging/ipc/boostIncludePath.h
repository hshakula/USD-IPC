/************************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
************************************************************************/

#ifndef BOOST_INCLUDE_PATH_H
#define BOOST_INCLUDE_PATH_H

#ifndef BOOST_NS
# define BOOST_NS boost
#endif // BOOST_NS

#define __STRINGIZE_PATHX(x) #x
#define __STRINGIZE_PATH(x) __STRINGIZE_PATHX(x)
#define BOOST_INCLUDE_PATH(suffix) __STRINGIZE_PATH(BOOST_NS/suffix)

#endif // BOOST_INCLUDE_PATH_H

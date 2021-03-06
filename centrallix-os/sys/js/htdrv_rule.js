// Copyright (C) 1998-2007 LightSys Technology Services, Inc.
//
// You may use these files and this library under the terms of the
// GNU Lesser General Public License, Version 2.1, contained in the
// included file "COPYING" or http://www.gnu.org/licenses/lgpl.txt.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.

function rl_init(node, ruletype, param)
    {
    node.kind = 'rl';
    node.ruletype = ruletype;

    for(var paramname in param)
	{
	node[paramname] = param[paramname];
	}

    return node;
    }

// Load indication
if (window.pg_scripts) pg_scripts['htdrv_rule.js'] = true;

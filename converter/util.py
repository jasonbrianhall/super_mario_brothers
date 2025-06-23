"""
Utility functions for 6502 Assembly Code Generator
"""

from typing import Dict, Set, Optional

# Global storage for comments and newlines
comment_map: Dict[int, str] = {}
newline_set: Set[int] = set()

def map_comment(line: int, comment: str):
    """Map a comment to a line number"""
    comment_map[line] = comment

def map_newline(line: int):
    """Map a newline to a line number"""
    newline_set.add(line)

def lookup_comment(line: int) -> Optional[str]:
    """Lookup and consume a comment for a line number"""
    if line in comment_map:
        comment = comment_map[line]
        # Remove the comment from the map so it can no longer be claimed
        # by a different token
        del comment_map[line]
        return comment
    return None

def clear_comments():
    """Clear all comments and newlines"""
    global comment_map, newline_set
    comment_map.clear()
    newline_set.clear()

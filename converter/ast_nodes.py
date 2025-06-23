"""
AST Node Classes for 6502 Assembly Code Generator - FIXED VERSION
"""

from enum import Enum
from typing import List, Optional, Union, Any

class AstType(Enum):
    AST_ROOT = "root"
    AST_LIST = "list"
    AST_DATA8 = "data8"
    AST_DATA16 = "data16"
    AST_DECL = "decl"
    AST_LABEL = "label"
    AST_NAME = "name"
    AST_CONST = "const"
    AST_IMMEDIATE = "immediate"
    AST_INDIRECT = "indirect"
    AST_INDEXED_X = "indexed_x"
    AST_INDEXED_Y = "indexed_y"
    AST_LOBYTE = "lobyte"
    AST_HIBYTE = "hibyte"
    AST_ADD = "add"
    AST_SUBTRACT = "subtract"
    AST_INSTRUCTION = "instruction"

class LabelType(Enum):
    LABEL_NONE = "none"
    LABEL_ALIAS = "alias"
    LABEL_CODE = "code"
    LABEL_DATA = "data"

class AstNode:
    def __init__(self, ast_type: AstType, value: Any = None):
        self.type = ast_type
        self.value = value
        self.parent: Optional['AstNode'] = None
        self.line_number = 0

class UnaryNode(AstNode):
    def __init__(self, ast_type: AstType, child: AstNode):
        super().__init__(ast_type)
        self.child = child

class BinaryNode(AstNode):
    def __init__(self, ast_type: AstType, lhs: AstNode, rhs: AstNode):
        super().__init__(ast_type)
        self.lhs = lhs
        self.rhs = rhs

class RootNode(AstNode):
    def __init__(self):
        super().__init__(AstType.AST_ROOT)
        self.children: List[AstNode] = []

class ListNode(AstNode):
    def __init__(self):
        super().__init__(AstType.AST_LIST)
        self.next: Optional['ListNode'] = None

class DeclNode(AstNode):
    def __init__(self, name: str, expression: AstNode):
        super().__init__(AstType.AST_DECL)
        self.value = name
        self.expression = expression

class LabelNode(AstNode):
    def __init__(self, name: str, child: AstNode):
        super().__init__(AstType.AST_LABEL)
        self.label_type = LabelType.LABEL_NONE
        self.value = name
        self.child = child

class InstructionNode(AstNode):
    def __init__(self, code: int, operand: Any = None):
        super().__init__(AstType.AST_INSTRUCTION)
        self.code = code
        self.value = operand

def cleanup_ast(root: RootNode):
    """Cleanup AST - FIXED to maintain source code order"""
    for i, node in enumerate(root.children):
        root.children[i] = cleanup_node(node)

def cleanup_node(node: AstNode):
    """Cleanup individual AST nodes - FIXED VERSION"""
    if node is None:
        return node
    
    if node.type == AstType.AST_LIST:
        # Don't reverse the list - keep source code order
        cleanup_list(node)
        return node
    elif node.type in [AstType.AST_DATA8, AstType.AST_DATA16]:
        node.value = cleanup_node(node.value)
    elif node.type == AstType.AST_DECL:
        node.expression = cleanup_node(node.expression)
    elif node.type == AstType.AST_LABEL:
        node.child = cleanup_node(node.child)
    
    return node

def cleanup_list(node: Optional[ListNode]):
    """Cleanup a list of nodes - FIXED to not reverse"""
    while node is not None:
        cleanup_node(node.value)
        node = node.next

def print_node(node: AstNode, indent: int = 0):
    """Print AST node for debugging"""
    if node is None:
        return
    
    indent_str = " " * indent
    
    if node.type == AstType.AST_LIST:
        print(f"{indent_str}list element:")
        print_node(node.value, indent + 4)
        print_node(node.next, indent)
    elif node.type == AstType.AST_DATA8:
        print(f"{indent_str}data:")
        print_node(node.value, indent + 4)
    elif node.type == AstType.AST_DATA16:
        print(f"{indent_str}data (16 bit):")
        print_node(node.value, indent + 4)
    elif node.type == AstType.AST_DECL:
        print(f"{indent_str}decl: {node.value} =")
        print_node(node.expression, indent + 4)
    elif node.type == AstType.AST_LABEL:
        print(f"{indent_str}label: {node.value}")
        print_node(node.child, indent + 4)
    elif node.type == AstType.AST_NAME:
        print(f"{indent_str}name: {node.value}")
    elif node.type == AstType.AST_CONST:
        print(f"{indent_str}constant: {node.value}")
    elif node.type == AstType.AST_IMMEDIATE:
        print(f"{indent_str}immediate:")
        print_node(node.child, indent + 4)
    elif node.type == AstType.AST_INDIRECT:
        print(f"{indent_str}indirect:")
        print_node(node.child, indent + 4)
    elif node.type == AstType.AST_INDEXED_X:
        print(f"{indent_str}indexed x:")
        print_node(node.child, indent + 4)
    elif node.type == AstType.AST_INDEXED_Y:
        print(f"{indent_str}indexed y:")
        print_node(node.child, indent + 4)
    elif node.type == AstType.AST_LOBYTE:
        print(f"{indent_str}low byte:")
        print_node(node.child, indent + 4)
    elif node.type == AstType.AST_HIBYTE:
        print(f"{indent_str}high byte:")
        print_node(node.child, indent + 4)
    elif node.type == AstType.AST_ADD:
        print(f"{indent_str}add:")
        print_node(node.lhs, indent + 4)
        print_node(node.rhs, indent + 4)
    elif node.type == AstType.AST_SUBTRACT:
        print(f"{indent_str}subtract:")
        print_node(node.lhs, indent + 4)
        print_node(node.rhs, indent + 4)
    elif node.type == AstType.AST_INSTRUCTION:
        print(f"{indent_str}instruction (line {node.line_number}) {node.code}:")
        print_node(node.value, indent + 4)

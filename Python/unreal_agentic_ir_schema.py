import json
from typing import List, Literal, Optional, Dict, Any, Union
from pydantic import BaseModel, Field

# --- Blackboard Keys ---
class IRBlackboardKey(BaseModel):
    name: str = Field(..., description="Name of the blackboard key")
    type: Literal["Object", "Vector", "Float", "Bool", "Class", "Int", "String", "Name"] = Field(..., description="Data type of the key")
    base_class: Optional[str] = Field(None, description="Base class path if type is Object or Class, e.g., '/Script/Engine.Actor'")

# --- Behavior Tree Nodes ---
class IRNodeBase(BaseModel):
    type: str = Field(..., description="Type of node (e.g., 'Sequence', 'Selector', 'Task', 'Decorator', 'Service')")
    name: Optional[str] = Field(None, description="Optional custom name for the node")

class IRDecorator(BaseModel):
    type: str = Field(..., description="Class name of the decorator, e.g., 'UBTDecorator_Blackboard'")
    properties: Dict[str, Any] = Field(default_factory=dict, description="Decorator properties like KeyName, FlowAbortMode, etc.")

class IRService(BaseModel):
    type: str = Field(..., description="Class name of the service, e.g., 'UBTService_RunEQS'")
    properties: Dict[str, Any] = Field(default_factory=dict, description="Service properties")

class IRTaskNode(IRNodeBase):
    type: Literal["Task"] = "Task"
    task_class: str = Field(..., description="Class of the task, e.g., 'UBTTask_MoveTo', 'UBTTask_Wait'")
    properties: Dict[str, Any] = Field(default_factory=dict, description="Task specific properties like BlackboardKey names, WaitTime")
    decorators: List[IRDecorator] = Field(default_factory=list)
    services: List[IRService] = Field(default_factory=list)

class IRCompositeNode(IRNodeBase):
    type: Literal["Sequence", "Selector", "SimpleParallel"] = Field(..., description="Composite node type")
    children: List[Union['IRCompositeNode', IRTaskNode]] = Field(default_factory=list, description="Children of the composite node")
    decorators: List[IRDecorator] = Field(default_factory=list)
    services: List[IRService] = Field(default_factory=list)

class IRRootNode(BaseModel):
    type: Literal["Root"] = "Root"
    child: Union['IRCompositeNode', IRTaskNode] = Field(..., description="The single child of the root node")
    blackboard_asset: str = Field(..., description="The Blackboard asset path to link to this tree")

# --- AI Asset Framework ---
class IRAIAsset(BaseModel):
    ir_version: str = Field("1.0.0", description="Version of the IR schema")
    asset_path: str = Field(..., description="Full path where the main Behavior Tree asset will be created (e.g., '/Game/AI/BT_Enemy')")
    blackboard_path: str = Field(..., description="Full path where the associated Blackboard asset will be created (e.g., '/Game/AI/BB_Enemy')")
    blackboard_keys: List[IRBlackboardKey] = Field(default_factory=list, description="Keys to initialize in the blackboard")
    tree_root: IRRootNode = Field(..., description="The root of the behavior tree")

# Update forward references
IRCompositeNode.model_rebuild()

def validate_ir_json(ir_json: dict) -> IRAIAsset:
    """Validates an IR JSON dictionary against the Pydantic schema."""
    return IRAIAsset.model_validate(ir_json)

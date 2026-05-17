"""
GravityOS — GravMind: On-Device AI Orchestrator
The JARVIS layer. Always running. 7B-param INT4-quantized LLM on NPU/GPU.
Handles all OS-level intent. No private data leaves device by default.

Copyright (c) 2026 GravityOS Contributors — MIT License
"""

import json
import hashlib
import time
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional
from pathlib import Path


class IntentType(Enum):
    """All OS-level intents GravMind can route."""
    LAUNCH_APP = auto()
    OPEN_FILE = auto()
    SEARCH = auto()
    SYSTEM_SETTING = auto()
    CREATE_APP = auto()       # "Make me a timer app"
    WINDOW_MANAGE = auto()    # "Tile my windows" / "Focus mode"
    COMMUNICATE = auto()      # "Call John" / "Send message"
    NAVIGATE = auto()         # "Open my project folder"
    AUTOMATE = auto()         # "Every morning, open email + calendar"
    MAINTAIN = auto()         # "Why is my fan loud?"
    SECURITY = auto()         # "What apps have network access?"
    ANSWER = auto()           # Direct knowledge question
    UNKNOWN = auto()


@dataclass
class Intent:
    """Parsed user intent with extracted entities."""
    type: IntentType
    confidence: float          # 0.0–1.0
    raw_input: str
    entities: dict = field(default_factory=dict)
    # Possible entities: app_name, file_path, query, setting_key,
    # setting_value, contact_name, time_expr, action_verb
    context_ids: list = field(default_factory=list)  # Related context memories
    timestamp: float = field(default_factory=time.time)


@dataclass
class GravMindConfig:
    """GravMind configuration."""
    model_path: str = "/sys/ai/models/gravmind-7b-int4.gguf"
    max_context_tokens: int = 8192
    temperature: float = 0.3        # Low temp for precise OS actions
    top_p: float = 0.9
    use_npu: bool = True             # Prefer NPU over GPU
    privacy_mode: bool = True        # No data leaves device
    voice_wake_word: str = "Hey Gravity"
    voice_enabled: bool = True
    max_response_time_ms: int = 500  # Target latency


class IntentParser:
    """
    Parses natural language into structured OS intents.
    Uses pattern matching + LLM fallback for ambiguous queries.
    """

    # Pattern-based fast path (no LLM needed)
    PATTERNS = {
        IntentType.LAUNCH_APP: [
            "open {app_name}", "launch {app_name}", "start {app_name}",
            "run {app_name}", "show {app_name}",
        ],
        IntentType.OPEN_FILE: [
            "open file {file_path}", "edit {file_path}",
            "show me {file_path}", "open {file_path}",
        ],
        IntentType.SEARCH: [
            "search {query}", "find {query}", "look for {query}",
            "where is {query}", "search for {query}",
        ],
        IntentType.SYSTEM_SETTING: [
            "set {setting_key} to {setting_value}",
            "change {setting_key}", "toggle {setting_key}",
            "turn on {setting_key}", "turn off {setting_key}",
            "enable {setting_key}", "disable {setting_key}",
        ],
        IntentType.WINDOW_MANAGE: [
            "tile windows", "stack windows", "focus mode",
            "split screen", "close all windows", "minimize all",
            "arrange windows", "snap {app_name} left",
        ],
        IntentType.CREATE_APP: [
            "make me {query}", "create {query}", "build {query}",
            "generate {query}", "I need {query}",
        ],
        IntentType.COMMUNICATE: [
            "call {contact_name}", "message {contact_name}",
            "email {contact_name}", "send {query} to {contact_name}",
        ],
        IntentType.MAINTAIN: [
            "why is {query}", "check {query}", "system health",
            "disk space", "battery", "performance", "temperature",
        ],
        IntentType.SECURITY: [
            "what apps have {query} access", "security scan",
            "show permissions", "lock {query}", "revoke {query}",
        ],
    }

    # Known GravityOS apps
    KNOWN_APPS = {
        "notes": "com.gravity.notes",
        "gravnote": "com.gravity.notes",
        "files": "com.gravity.files",
        "gravfiles": "com.gravity.files",
        "browser": "com.gravity.browser",
        "gravbrowser": "com.gravity.browser",
        "terminal": "com.gravity.terminal",
        "gravterminal": "com.gravity.terminal",
        "media": "com.gravity.media",
        "gravmedia": "com.gravity.media",
        "connect": "com.gravity.connect",
        "settings": "com.gravity.settings",
        "mail": "com.gravity.connect",
        "camera": "com.gravity.camera",
    }

    def parse(self, raw_input: str) -> Intent:
        """Parse natural language input into a structured Intent."""
        text = raw_input.strip().lower()

        # Fast path: pattern matching
        for intent_type, patterns in self.PATTERNS.items():
            for pattern in patterns:
                entities = self._match_pattern(text, pattern)
                if entities is not None:
                    # Resolve app names
                    if "app_name" in entities:
                        app_key = entities["app_name"].lower().strip()
                        if app_key in self.KNOWN_APPS:
                            entities["app_id"] = self.KNOWN_APPS[app_key]

                    return Intent(
                        type=intent_type,
                        confidence=0.95,
                        raw_input=raw_input,
                        entities=entities,
                    )

        # Slow path: LLM inference for ambiguous queries
        return self._llm_classify(raw_input)

    def _match_pattern(self, text: str, pattern: str) -> Optional[dict]:
        """Simple pattern matching with {entity} extraction."""
        parts = pattern.split("{")
        if not text.startswith(parts[0].strip()):
            return None

        entities = {}
        remaining = text[len(parts[0].strip()):].strip()

        for i in range(1, len(parts)):
            entity_and_rest = parts[i].split("}", 1)
            entity_name = entity_and_rest[0]
            suffix = entity_and_rest[1].strip() if len(entity_and_rest) > 1 else ""

            if suffix:
                idx = remaining.find(suffix)
                if idx == -1:
                    return None
                entities[entity_name] = remaining[:idx].strip()
                remaining = remaining[idx + len(suffix):].strip()
            else:
                entities[entity_name] = remaining.strip()
                remaining = ""

        return entities if not remaining or len(entities) > 0 else None

    def _llm_classify(self, raw_input: str) -> Intent:
        """Fallback: use on-device LLM to classify intent."""
        # In real implementation: call gravmind inference
        # For now, return ANSWER type for unmatched queries
        return Intent(
            type=IntentType.ANSWER,
            confidence=0.7,
            raw_input=raw_input,
            entities={"query": raw_input},
        )


class ContextMemory:
    """
    GravMemory — Persistent semantic vector memory.
    FAISS on NVMe. Fully encrypted. User-owned.
    Remembers context, preferences, patterns across all sessions.
    """

    @dataclass
    class MemoryEntry:
        id: str
        content: str
        embedding: list         # 384-dim vector (MiniLM)
        metadata: dict
        timestamp: float
        access_count: int = 0
        source_app: str = ""
        privacy_level: str = "local"  # "local" | "cloud_sync"

    def __init__(self, db_path: str = "/var/gravity/ai/memory.db"):
        self.db_path = db_path
        self.entries: list = []
        self.session_context: list = []  # Current session

    def remember(self, content: str, metadata: dict = None,
                 source_app: str = "", privacy: str = "local") -> str:
        """Store a memory entry with semantic embedding."""
        entry_id = hashlib.sha256(
            f"{content}{time.time()}".encode()
        ).hexdigest()[:16]

        entry = self.MemoryEntry(
            id=entry_id,
            content=content,
            embedding=self._compute_embedding(content),
            metadata=metadata or {},
            timestamp=time.time(),
            source_app=source_app,
            privacy_level=privacy,
        )
        self.entries.append(entry)
        self.session_context.append(entry_id)
        return entry_id

    def recall(self, query: str, top_k: int = 5) -> list:
        """Semantic search — find most relevant memories."""
        query_vec = self._compute_embedding(query)
        scored = []
        for entry in self.entries:
            score = self._cosine_similarity(query_vec, entry.embedding)
            scored.append((score, entry))
        scored.sort(key=lambda x: x[0], reverse=True)
        results = scored[:top_k]
        for _, entry in results:
            entry.access_count += 1
        return [(s, e.content, e.metadata) for s, e in results]

    def get_session_context(self) -> list:
        """Get all memories from current session."""
        return [e for e in self.entries if e.id in self.session_context]

    def _compute_embedding(self, text: str) -> list:
        """Compute semantic embedding vector. 
        Real impl: MiniLM-L6 on NPU. Placeholder: hash-based."""
        h = hashlib.sha256(text.encode()).digest()
        return [((b & 0xFF) - 128) / 128.0 for b in h[:32]]

    @staticmethod
    def _cosine_similarity(a: list, b: list) -> float:
        dot = sum(x * y for x, y in zip(a, b))
        norm_a = sum(x * x for x in a) ** 0.5
        norm_b = sum(x * x for x in b) ** 0.5
        return dot / (norm_a * norm_b + 1e-8)


class AppGenerator:
    """
    App Generation Engine — describe a tool, AI builds a .grav micro-app.
    Natural language → sandboxed app in under 10 seconds.
    """

    TEMPLATES = {
        "timer": {
            "name": "Timer",
            "icon": "⏱",
            "code_template": "timer_app",
        },
        "calculator": {
            "name": "Calculator",
            "icon": "🔢",
            "code_template": "calc_app",
        },
        "todo": {
            "name": "Todo List",
            "icon": "✓",
            "code_template": "todo_app",
        },
        "notes": {
            "name": "Quick Notes",
            "icon": "📝",
            "code_template": "notes_app",
        },
        "converter": {
            "name": "Unit Converter",
            "icon": "⟷",
            "code_template": "converter_app",
        },
    }

    def generate(self, description: str) -> dict:
        """Generate a micro-app from natural language description."""
        # Match against templates
        desc_lower = description.lower()
        for key, template in self.TEMPLATES.items():
            if key in desc_lower:
                return {
                    "status": "generated",
                    "app": template,
                    "format": ".grav",
                    "sandboxed": True,
                    "capabilities": ["display"],
                    "source": "template",
                }

        # LLM-generated app (real impl: code generation model)
        return {
            "status": "generated",
            "app": {
                "name": f"Custom: {description[:30]}",
                "icon": "⚡",
                "code_template": "custom",
            },
            "format": ".grav",
            "sandboxed": True,
            "capabilities": ["display"],
            "source": "ai_generated",
        }


class GravFlow:
    """
    Workflow prediction — learns patterns, auto-arranges workspaces,
    suggests apps before user asks.
    """

    @dataclass
    class WorkflowPattern:
        apps: list              # Ordered app launches
        time_of_day: str        # "morning" | "afternoon" | "evening"
        day_of_week: int        # 0=Mon ... 6=Sun
        frequency: int          # How often this pattern occurs
        layout: str             # "tiled" | "stacked" | "focused"

    def __init__(self):
        self.patterns: list = []
        self.current_session_apps: list = []

    def record_app_launch(self, app_id: str):
        """Track app launches for pattern learning."""
        self.current_session_apps.append({
            "app_id": app_id,
            "timestamp": time.time(),
        })

    def predict_next_app(self) -> Optional[str]:
        """Predict what app the user will open next."""
        if not self.current_session_apps:
            return None

        last_app = self.current_session_apps[-1]["app_id"]
        # Find patterns where last_app appears
        for pattern in self.patterns:
            for i, app in enumerate(pattern.apps):
                if app == last_app and i + 1 < len(pattern.apps):
                    return pattern.apps[i + 1]
        return None

    def suggest_layout(self) -> dict:
        """Suggest window layout based on current apps."""
        app_count = len(self.current_session_apps)
        if app_count <= 1:
            return {"layout": "focused", "arrangement": "fullscreen"}
        elif app_count == 2:
            return {"layout": "split", "arrangement": "50-50"}
        else:
            return {"layout": "tiled", "arrangement": "grid"}


class PCMaintenance:
    """
    AI PC Maintenance — monitors system health, auto-fixes when safe.
    CPU/GPU temps, RAM pressure, disk SMART, startup perf, process bloat.
    """

    @dataclass
    class SystemHealth:
        cpu_temp_c: float = 45.0
        gpu_temp_c: float = 40.0
        ram_usage_pct: float = 35.0
        disk_health_pct: float = 98.0
        battery_pct: float = 85.0
        uptime_hours: float = 2.5
        process_count: int = 42
        startup_time_s: float = 3.2

    def __init__(self):
        self.health = self.SystemHealth()
        self.alerts: list = []

    def check_all(self) -> list:
        """Run all health checks. Returns list of issues."""
        issues = []

        if self.health.cpu_temp_c > 85:
            issues.append({
                "severity": "warning",
                "component": "CPU",
                "message": f"High temperature: {self.health.cpu_temp_c}°C",
                "action": "Throttling CPU to reduce heat",
                "auto_fixable": True,
            })

        if self.health.ram_usage_pct > 90:
            issues.append({
                "severity": "warning",
                "component": "RAM",
                "message": f"High memory pressure: {self.health.ram_usage_pct}%",
                "action": "Suspending background apps",
                "auto_fixable": True,
            })

        if self.health.disk_health_pct < 50:
            issues.append({
                "severity": "critical",
                "component": "Disk",
                "message": f"Disk health declining: {self.health.disk_health_pct}%",
                "action": "Recommend backup immediately",
                "auto_fixable": False,
            })

        if not issues:
            issues.append({
                "severity": "info",
                "component": "System",
                "message": "All systems nominal",
                "auto_fixable": False,
            })

        return issues


# ═══════ GravMind Orchestrator ═══════
class GravMind:
    """
    Main AI orchestrator. Combines intent parsing, context memory,
    app generation, workflow prediction, and system maintenance.
    """

    def __init__(self, config: GravMindConfig = None):
        self.config = config or GravMindConfig()
        self.intent_parser = IntentParser()
        self.context = ContextMemory()
        self.app_gen = AppGenerator()
        self.flow = GravFlow()
        self.maintenance = PCMaintenance()
        self.running = True

    def process_input(self, user_input: str) -> dict:
        """Process natural language input and return action."""
        # Parse intent
        intent = self.intent_parser.parse(user_input)

        # Store in context memory
        self.context.remember(
            content=user_input,
            metadata={"intent": intent.type.name, "entities": intent.entities},
            source_app="gravmind",
        )

        # Route to appropriate handler
        handlers = {
            IntentType.LAUNCH_APP: self._handle_launch,
            IntentType.OPEN_FILE: self._handle_open_file,
            IntentType.SEARCH: self._handle_search,
            IntentType.SYSTEM_SETTING: self._handle_setting,
            IntentType.CREATE_APP: self._handle_create_app,
            IntentType.WINDOW_MANAGE: self._handle_window,
            IntentType.MAINTAIN: self._handle_maintain,
            IntentType.ANSWER: self._handle_answer,
        }

        handler = handlers.get(intent.type, self._handle_answer)
        result = handler(intent)
        result["intent"] = intent.type.name
        result["confidence"] = intent.confidence
        return result

    def _handle_launch(self, intent: Intent) -> dict:
        app_name = intent.entities.get("app_name", "")
        app_id = intent.entities.get("app_id", "")
        self.flow.record_app_launch(app_id or app_name)
        return {"action": "launch", "app_id": app_id, "app_name": app_name}

    def _handle_open_file(self, intent: Intent) -> dict:
        return {"action": "open_file", "path": intent.entities.get("file_path", "")}

    def _handle_search(self, intent: Intent) -> dict:
        query = intent.entities.get("query", "")
        results = self.context.recall(query, top_k=5)
        return {"action": "search", "query": query, "memory_results": len(results)}

    def _handle_setting(self, intent: Intent) -> dict:
        return {
            "action": "setting",
            "key": intent.entities.get("setting_key", ""),
            "value": intent.entities.get("setting_value", ""),
        }

    def _handle_create_app(self, intent: Intent) -> dict:
        desc = intent.entities.get("query", intent.raw_input)
        app = self.app_gen.generate(desc)
        return {"action": "create_app", "app": app}

    def _handle_window(self, intent: Intent) -> dict:
        layout = self.flow.suggest_layout()
        return {"action": "window_manage", "layout": layout}

    def _handle_maintain(self, intent: Intent) -> dict:
        issues = self.maintenance.check_all()
        return {"action": "maintain", "health": issues}

    def _handle_answer(self, intent: Intent) -> dict:
        query = intent.entities.get("query", intent.raw_input)
        return {"action": "answer", "query": query,
                "response": f"[GravMind would answer: {query}]"}


if __name__ == "__main__":
    gm = GravMind()
    test_inputs = [
        "open browser",
        "search my documents for quarterly report",
        "make me a pomodoro timer",
        "tile windows",
        "why is my fan so loud",
        "what's the weather today",
        "set dark mode",
    ]
    for inp in test_inputs:
        result = gm.process_input(inp)
        print(f"\n> {inp}")
        print(f"  → {json.dumps(result, indent=4, default=str)}")

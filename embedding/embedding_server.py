from flask import Flask, request, jsonify
from sentence_transformers import SentenceTransformer

app = Flask(__name__)
model = SentenceTransformer("sentence-transformers/all-mpnet-base-v2")

@app.route("/embed", methods=["POST"])
def embed():
    try:
        text = request.json["text"]
        print("sentence transform request received for: " + text)
        
        embedding = model.encode(text)
        
        return jsonify({
            "embedding": embedding.tolist()
        })
    except Exception as e:
        print("Exception raised on flask server:", str(e))
        return jsonify({"error": str(e)}), 500  # Internal Server Error
    
    return jsonify({
        "embedding": embedding.tolist()
    })

app.run(port=5000)